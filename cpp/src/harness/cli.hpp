#pragma once

#include "../book/engine.hpp"
#include "../constants.hpp"
#include "../env.hpp"
#include "../fix/codec.hpp"
#include "../md/binary.hpp"
#include "../md/ticks.hpp"
#include "../risk/engine.hpp"
#include "../workload.hpp"
#include "json.hpp"
#include "percentiles.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

struct Config {
    std::size_t ops = 1'000'000;
    std::size_t warmup = 50'000;
    uint64_t seed = c::DEFAULT_SEED;
    int32_t symbols = 32;
    int32_t accounts = 256;
    std::string output = "results/cpp.json";
    std::string runtime_name = "unknown";
    std::string benches = "all";
    bool help = false;

    bool want(const std::string& name) const {
        if (benches == "all") {
            return true;
        }
        std::string rest = benches;
        while (!rest.empty()) {
            auto comma = rest.find(',');
            std::string p = rest.substr(0, comma);
            while (!p.empty() && p.front() == ' ') {
                p.erase(p.begin());
            }
            while (!p.empty() && p.back() == ' ') {
                p.pop_back();
            }
            if (p == name) {
                return true;
            }
            if (comma == std::string::npos) {
                break;
            }
            rest = rest.substr(comma + 1);
        }
        return false;
    }
};

inline uint64_t parse_seed(const std::string& s) {
    if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        return std::stoull(s.substr(2), nullptr, 16);
    }
    return std::stoull(s, nullptr, 10);
}

inline Config parse_args(int argc, char** argv) {
    Config c;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        auto need = [&](const char* name) -> std::string {
            if (i + 1 >= argc) {
                throw std::runtime_error(std::string("missing value for ") + name);
            }
            return argv[++i];
        };
        if (a == "-h" || a == "--help") {
            c.help = true;
        } else if (a == "--ops") {
            c.ops = static_cast<std::size_t>(std::stoull(need("--ops")));
        } else if (a == "--warmup") {
            c.warmup = static_cast<std::size_t>(std::stoull(need("--warmup")));
        } else if (a == "--seed") {
            c.seed = parse_seed(need("--seed"));
        } else if (a == "--symbols") {
            c.symbols = std::stoi(need("--symbols"));
        } else if (a == "--accounts") {
            c.accounts = std::stoi(need("--accounts"));
        } else if (a == "--output") {
            c.output = need("--output");
        } else if (a == "--runtime-name") {
            c.runtime_name = need("--runtime-name");
        } else if (a == "--bench") {
            c.benches = need("--bench");
        } else {
            throw std::runtime_error("unknown arg: " + a);
        }
    }
    if (c.ops == 0 || c.symbols <= 0 || c.accounts <= 0) {
        throw std::runtime_error("ops/symbols/accounts must be > 0");
    }
    return c;
}

inline void print_help() {
    std::cout <<
        "cpp-bench — equities C++ harness\n"
        "  --ops N              operations per bench (default 1000000)\n"
        "  --warmup N           warmup ops (default 50000)\n"
        "  --seed HEX_OR_DEC    default 0x0C0FFEE123456789\n"
        "  --symbols N          default 32\n"
        "  --accounts N         default 256\n"
        "  --bench all|name,... order-book,fix-parse,risk,market-data,binary-md\n"
        "  --output PATH\n"
        "  --runtime-name NAME  docker|podman|host\n";
}

inline uint64_t now_ns() {
    using clock = std::chrono::steady_clock;
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(clock::now().time_since_epoch()).count());
}

inline void print_result(const BenchResult& r) {
    std::cout << "=== " << r.name << " ===  " << static_cast<long long>(r.throughput_ops_s)
              << " ops/s  p50=" << r.p50_ns << " ns  p99=" << r.p99_ns
              << " ns  checksum=" << r.checksum << "\n";
}

inline BenchResult run_order_book(const Config& cfg) {
    std::cout << "generating order-book workload\n";
    Workload w = Workload::generate(cfg.seed, cfg.ops, cfg.symbols, cfg.accounts);
    std::size_t warmup = cfg.warmup < cfg.ops ? cfg.warmup : cfg.ops;
    std::cout << "warming order-book (" << warmup << " ops)\n";
    {
        OrderBookEngine throwaway(cfg.symbols, static_cast<int32_t>(cfg.ops));
        for (std::size_t i = 0; i < warmup; i++) {
            throwaway.apply(w, i);
        }
    }
    std::cout << "measuring order-book\n";
    OrderBookEngine eng(cfg.symbols, static_cast<int32_t>(cfg.ops));
    std::vector<uint64_t> samples(cfg.ops);
    uint64_t t0 = now_ns();
    for (std::size_t i = 0; i < cfg.ops; i++) {
        uint64_t s = now_ns();
        eng.apply(w, i);
        samples[i] = now_ns() - s;
    }
    uint64_t elapsed = now_ns() - t0;
    eng.finish();
    sort_in_place(samples);
    BenchResult r = BenchResult::make("order-book", cfg.ops, elapsed, eng.checksum, samples)
                        .stat("fills", static_cast<int64_t>(eng.fill_count))
                        .stat("fill_qty", eng.fill_qty)
                        .stat("cancel_hits", static_cast<int64_t>(eng.cancel_hits))
                        .stat("cancel_misses", static_cast<int64_t>(eng.cancel_misses))
                        .stat("ioc_killed", eng.ioc_killed)
                        .stat("resting_adds", static_cast<int64_t>(eng.rest_count));
    print_result(r);
    return r;
}

inline BenchResult run_fix(const Config& cfg) {
    std::cout << "generating FIX messages\n";
    FixBench bench(cfg.seed, cfg.ops, cfg.symbols);
    std::size_t warmup = cfg.warmup < cfg.ops ? cfg.warmup : cfg.ops;
    std::cout << "warming fix-parse\n";
    for (std::size_t i = 0; i < warmup; i++) {
        bench.parse(i);
    }
    bench.checksum = 0;
    std::cout << "measuring fix-parse\n";
    std::vector<uint64_t> samples(cfg.ops);
    uint64_t t0 = now_ns();
    for (std::size_t i = 0; i < cfg.ops; i++) {
        uint64_t s = now_ns();
        bench.parse(i);
        samples[i] = now_ns() - s;
    }
    uint64_t elapsed = now_ns() - t0;
    sort_in_place(samples);
    BenchResult r = BenchResult::make("fix-parse", cfg.ops, elapsed, bench.checksum, samples)
                        .stat("messages", static_cast<int64_t>(cfg.ops))
                        .stat("bytes", static_cast<int64_t>(bench.arena.size()));
    print_result(r);
    return r;
}

inline BenchResult run_risk(const Config& cfg) {
    std::cout << "generating risk workload\n";
    Workload w = Workload::generate(cfg.seed, cfg.ops, cfg.symbols, cfg.accounts);
    std::size_t warmup = cfg.warmup < cfg.ops ? cfg.warmup : cfg.ops;
    std::cout << "warming risk\n";
    {
        RiskEngine throwaway(cfg.accounts, cfg.symbols);
        for (std::size_t i = 0; i < warmup; i++) {
            throwaway.apply(w, i);
        }
    }
    std::cout << "measuring risk\n";
    RiskEngine eng(cfg.accounts, cfg.symbols);
    std::vector<uint64_t> samples(cfg.ops);
    uint64_t t0 = now_ns();
    for (std::size_t i = 0; i < cfg.ops; i++) {
        uint64_t s = now_ns();
        eng.apply(w, i);
        samples[i] = now_ns() - s;
    }
    uint64_t elapsed = now_ns() - t0;
    eng.finish();
    sort_in_place(samples);
    BenchResult r = BenchResult::make("risk", cfg.ops, elapsed, eng.checksum, samples)
                        .stat("accepts", static_cast<int64_t>(eng.accepts))
                        .stat("rejects", static_cast<int64_t>(eng.rejects));
    print_result(r);
    return r;
}

inline BenchResult run_market_data(const Config& cfg) {
    std::cout << "generating market-data ticks\n";
    MarketDataBench bench(cfg.seed, cfg.ops, cfg.symbols);
    std::size_t warmup = cfg.warmup < cfg.ops ? cfg.warmup : cfg.ops;
    std::cout << "warming market-data\n";
    for (std::size_t i = 0; i < warmup; i++) {
        bench.apply(i);
    }
    bench.reset_aggs();
    std::cout << "measuring market-data\n";
    std::vector<uint64_t> samples(cfg.ops);
    uint64_t t0 = now_ns();
    for (std::size_t i = 0; i < cfg.ops; i++) {
        uint64_t s = now_ns();
        bench.apply(i);
        samples[i] = now_ns() - s;
    }
    uint64_t elapsed = now_ns() - t0;
    bench.finish();
    sort_in_place(samples);
    BenchResult r = BenchResult::make("market-data", cfg.ops, elapsed, bench.checksum, samples)
                        .stat("ticks", static_cast<int64_t>(cfg.ops));
    print_result(r);
    return r;
}

inline BenchResult run_binary_md(const Config& cfg) {
    std::cout << "generating binary ticks\n";
    BinaryMdBench bench(cfg.seed, cfg.ops, cfg.symbols);
    std::size_t warmup = cfg.warmup < cfg.ops ? cfg.warmup : cfg.ops;
    std::cout << "warming binary-md\n";
    for (std::size_t i = 0; i < warmup; i++) {
        bench.apply(i);
    }
    bench.reset_aggs();
    std::cout << "measuring binary-md\n";
    std::vector<uint64_t> samples(cfg.ops);
    uint64_t t0 = now_ns();
    for (std::size_t i = 0; i < cfg.ops; i++) {
        uint64_t s = now_ns();
        bench.apply(i);
        samples[i] = now_ns() - s;
    }
    uint64_t elapsed = now_ns() - t0;
    bench.finish();
    sort_in_place(samples);
    BenchResult r = BenchResult::make("binary-md", cfg.ops, elapsed, bench.checksum, samples)
                        .stat("ticks", static_cast<int64_t>(cfg.ops))
                        .stat("bytes", static_cast<int64_t>(cfg.ops * REC));
    print_result(r);
    return r;
}

inline int run_cli(int argc, char** argv) {
    Config cfg = parse_args(argc, argv);
    if (cfg.help) {
        print_help();
        return 0;
    }
    std::cout << "cpp-bench ops=" << cfg.ops << " warmup=" << cfg.warmup << " seed=" << cfg.seed
              << " symbols=" << cfg.symbols << " accounts=" << cfg.accounts
              << " benches=" << cfg.benches << "\n";
    EnvInfo env = collect_env();
    std::vector<BenchResult> results;
    if (cfg.want("order-book")) {
        results.push_back(run_order_book(cfg));
    }
    if (cfg.want("fix-parse")) {
        results.push_back(run_fix(cfg));
    }
    if (cfg.want("risk")) {
        results.push_back(run_risk(cfg));
    }
    if (cfg.want("market-data")) {
        results.push_back(run_market_data(cfg));
    }
    if (cfg.want("binary-md")) {
        results.push_back(run_binary_md(cfg));
    }
    write_json(cfg.output, env, cfg.runtime_name, cfg.seed, cfg.ops, cfg.warmup, cfg.symbols, cfg.accounts, results);
    std::cout << "wrote " << cfg.output << "\n";
    return 0;
}
