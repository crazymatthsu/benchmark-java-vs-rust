#include "engine.hpp"
#include "symbol.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

struct Args {
    std::string cmd;
    std::string exchanges = "data/exchanges.txt";
    std::string state = "build/state";
    std::string symbol;
    int venue = 1;
    bool integer = false;
    uint64_t ops = 1'000'000;
    uint64_t seed = 1;
};

static void help() {
    std::cout <<
        "hss — US 11,948,192 high-speed symbol storage (software model)\n"
        "  generate --exchanges FILE --out DIR\n"
        "  update   --exchanges FILE --state DIR     (rebuild inactive bank, switch)\n"
        "  lookup   --state DIR --symbol TICKER [--venue N] [--int]\n"
        "  bench    --state DIR [--ops N]\n";
}

static Args parse(int argc, char** argv) {
    Args a;
    if (argc < 2) {
        a.cmd = "help";
        return a;
    }
    a.cmd = argv[1];
    for (int i = 2; i < argc; i++) {
        std::string x = argv[i];
        auto need = [&] {
            if (i + 1 >= argc) {
                throw std::runtime_error("missing value after " + x);
            }
            return std::string(argv[++i]);
        };
        if (x == "--exchanges") {
            a.exchanges = need();
        } else if (x == "--out" || x == "--state") {
            a.state = need();
        } else if (x == "--symbol") {
            a.symbol = need();
        } else if (x == "--venue") {
            a.venue = std::stoi(need());
        } else if (x == "--int") {
            a.integer = true;
        } else if (x == "--ops") {
            a.ops = std::stoull(need());
        } else if (x == "--seed") {
            a.seed = std::stoull(need(), nullptr, 0);
        } else if (x == "-h" || x == "--help") {
            a.cmd = "help";
        } else {
            throw std::runtime_error("unknown arg " + x);
        }
    }
    return a;
}

static int cmd_generate(const Args& a, bool update) {
    Engine eng;
    if (update) {
        eng.load_state(a.state);
        eng.update_from(a.exchanges, a.seed + 99);
        std::cout << "intraday bank switch complete\n";
    } else {
        eng.generate_from(a.exchanges, a.seed);
    }
    eng.write_outputs(a.state);
    eng.dump_summary(std::cout);
    std::cout << "wrote " << a.state << "\n";
    return 0;
}

static int cmd_lookup(const Args& a) {
    Engine eng;
    eng.load_state(a.state);
    const Instrument* ins = eng.lookup_symbol(a.symbol, static_cast<uint8_t>(a.venue), a.integer);
    if (!ins) {
        std::cerr << "not found: " << a.symbol << " venue " << a.venue << "\n";
        return 1;
    }
    std::cout << "id=" << ins->id
              << " display=" << ins->display
              << " venue=" << static_cast<int>(ins->venue)
              << " last_px=" << ins->last_px
              << " halt=" << static_cast<int>(ins->halted)
              << " etb=" << static_cast<int>(ins->easy_to_borrow) << "\n";
    return 0;
}

static int cmd_bench(const Args& a) {
    Engine eng;
    eng.load_state(a.state);
    if (eng.table.size() == 0) {
        std::cerr << "empty table\n";
        return 1;
    }
    std::vector<uint64_t> keys;
    keys.reserve(eng.table.size());
    for (const auto& ins : eng.table.rows) {
        keys.push_back(ins.packed);
    }
    uint32_t sink = 0;
    const auto t0 = std::chrono::steady_clock::now();
    for (uint64_t i = 0; i < a.ops; i++) {
        sink += eng.banks.lookup(keys[i % keys.size()]);
    }
    asm volatile("" ::"r"(sink) : "memory");
    const auto t1 = std::chrono::steady_clock::now();
    const double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
    const double ops_s = a.ops / (ns / 1e9);
    std::cout << "lookups=" << a.ops
              << " symbols=" << keys.size()
              << " " << static_cast<long long>(ops_s) << " lookups/s"
              << " sink=" << sink << "\n";
    return 0;
}

int main(int argc, char** argv) {
    try {
        const Args a = parse(argc, argv);
        if (a.cmd == "help" || a.cmd == "-h") {
            help();
            return 0;
        }
        if (a.cmd == "generate") {
            return cmd_generate(a, false);
        }
        if (a.cmd == "update") {
            return cmd_generate(a, true);
        }
        if (a.cmd == "lookup") {
            return cmd_lookup(a);
        }
        if (a.cmd == "bench") {
            return cmd_bench(a);
        }
        help();
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << "\n";
        return 1;
    }
}
