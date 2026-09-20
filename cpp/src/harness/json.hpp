#pragma once

#include "../env.hpp"
#include "percentiles.hpp"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

struct BenchResult {
    std::string name;
    uint64_t ops = 0;
    uint64_t elapsed_ns = 0;
    double throughput_ops_s = 0;
    std::string checksum;
    uint64_t min_ns = 0;
    uint64_t p50_ns = 0;
    uint64_t p90_ns = 0;
    uint64_t p99_ns = 0;
    uint64_t p999_ns = 0;
    uint64_t max_ns = 0;
    std::vector<std::pair<std::string, int64_t>> stats;

    static BenchResult make(
        const std::string& name, uint64_t ops, uint64_t elapsed_ns, uint64_t checksum,
        const std::vector<uint64_t>& sorted) {
        BenchResult b;
        b.name = name;
        b.ops = ops;
        b.elapsed_ns = elapsed_ns;
        b.throughput_ops_s = elapsed_ns == 0 ? 0.0 : static_cast<double>(ops) / (static_cast<double>(elapsed_ns) / 1e9);
        b.checksum = std::to_string(checksum);
        b.min_ns = min_v(sorted);
        b.p50_ns = pct(sorted, 0.50);
        b.p90_ns = pct(sorted, 0.90);
        b.p99_ns = pct(sorted, 0.99);
        b.p999_ns = pct(sorted, 0.999);
        b.max_ns = max_v(sorted);
        return b;
    }

    BenchResult& stat(const std::string& k, int64_t v) {
        stats.emplace_back(k, v);
        return *this;
    }
};

inline std::string escape_json(const std::string& s) {
    std::string o;
    for (char ch : s) {
        if (ch == '\\' || ch == '"') {
            o.push_back('\\');
        }
        o.push_back(ch);
    }
    return o;
}

inline void field_s(std::ostringstream& sb, const std::string& k, const std::string& v, bool comma, int indent) {
    sb << std::string(static_cast<std::size_t>(indent), ' ') << '"' << k << "\": \"" << escape_json(v) << '"';
    sb << (comma ? ",\n" : "\n");
}

inline void field_i(std::ostringstream& sb, const std::string& k, int64_t v, bool comma, int indent) {
    sb << std::string(static_cast<std::size_t>(indent), ' ') << '"' << k << "\": " << v;
    sb << (comma ? ",\n" : "\n");
}

inline void field_u(std::ostringstream& sb, const std::string& k, uint64_t v, bool comma, int indent) {
    sb << std::string(static_cast<std::size_t>(indent), ' ') << '"' << k << "\": " << v;
    sb << (comma ? ",\n" : "\n");
}

inline void field_f(std::ostringstream& sb, const std::string& k, double v, bool comma, int indent) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.6f", v);
    sb << std::string(static_cast<std::size_t>(indent), ' ') << '"' << k << "\": " << buf;
    sb << (comma ? ",\n" : "\n");
}

inline void write_json(
    const std::filesystem::path& path,
    const EnvInfo& env,
    const std::string& container_runtime,
    uint64_t seed,
    uint64_t ops,
    uint64_t warmup,
    int32_t symbols,
    int32_t accounts,
    const std::vector<BenchResult>& benches) {
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }
    std::ostringstream sb;
    sb << "{\n";
    field_i(sb, "schema_version", 1, true, 2);
    field_s(sb, "language", "cpp", true, 2);
    field_s(sb, "runtime", env.runtime, true, 2);
    field_s(sb, "os", env.os, true, 2);
    field_s(sb, "arch", env.arch, true, 2);
    field_u(sb, "cpus", env.cpus, true, 2);
    field_u(sb, "mem_bytes", env.mem_bytes, true, 2);
    field_s(sb, "os_pretty", env.os_pretty, true, 2);
    field_s(sb, "cpu_model", env.cpu_model, true, 2);
    field_s(sb, "container_runtime", container_runtime, true, 2);
    field_s(sb, "seed", std::to_string(seed), true, 2);
    field_u(sb, "ops", ops, true, 2);
    field_u(sb, "warmup", warmup, true, 2);
    field_i(sb, "symbols", symbols, true, 2);
    field_i(sb, "accounts", accounts, true, 2);
    sb << "  \"benchmarks\": [\n";
    for (std::size_t i = 0; i < benches.size(); i++) {
        const BenchResult& b = benches[i];
        bool comma = i + 1 < benches.size();
        sb << "    {\n";
        field_s(sb, "name", b.name, true, 6);
        field_u(sb, "ops", b.ops, true, 6);
        field_u(sb, "elapsed_ns", b.elapsed_ns, true, 6);
        field_f(sb, "throughput_ops_s", b.throughput_ops_s, true, 6);
        field_s(sb, "checksum", b.checksum, true, 6);
        sb << "      \"latency_ns\": {\n";
        field_u(sb, "min", b.min_ns, true, 8);
        field_u(sb, "p50", b.p50_ns, true, 8);
        field_u(sb, "p90", b.p90_ns, true, 8);
        field_u(sb, "p99", b.p99_ns, true, 8);
        field_u(sb, "p999", b.p999_ns, true, 8);
        field_u(sb, "max", b.max_ns, false, 8);
        sb << "      },\n";
        sb << "      \"stats\": {\n";
        for (std::size_t n = 0; n < b.stats.size(); n++) {
            field_i(sb, b.stats[n].first, b.stats[n].second, n + 1 < b.stats.size(), 8);
        }
        sb << "      }\n";
        sb << "    " << (comma ? "},\n" : "}\n");
    }
    sb << "  ]\n";
    sb << "}\n";
    std::ofstream out(path);
    out << sb.str();
}
