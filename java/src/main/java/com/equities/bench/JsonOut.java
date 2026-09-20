package com.equities.bench;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.List;
import java.util.Locale;
import java.util.Map;

public final class JsonOut {
    private JsonOut() {}

    public static void write(
            Path path,
            EnvInfo env,
            String containerRuntime,
            long seed,
            int ops,
            int warmup,
            int symbols,
            int accounts,
            List<BenchResult> benches) throws IOException {
        StringBuilder sb = new StringBuilder();
        sb.append("{\n");
        field(sb, "schema_version", 1, true);
        field(sb, "language", "java", true);
        field(sb, "runtime", env.runtime, true);
        field(sb, "os", env.os, true);
        field(sb, "arch", env.arch, true);
        field(sb, "cpus", env.cpus, true);
        field(sb, "mem_bytes", env.memBytes, true);
        field(sb, "os_pretty", env.osPretty, true);
        field(sb, "cpu_model", env.cpuModel, true);
        field(sb, "container_runtime", containerRuntime, true);
        field(sb, "seed", Long.toUnsignedString(seed), true);
        field(sb, "ops", ops, true);
        field(sb, "warmup", warmup, true);
        field(sb, "symbols", symbols, true);
        field(sb, "accounts", accounts, true);
        sb.append("  \"benchmarks\": [\n");
        for (int i = 0; i < benches.size(); i++) {
            writeBench(sb, benches.get(i), i + 1 < benches.size());
        }
        sb.append("  ]\n");
        sb.append("}\n");
        if (path.getParent() != null) {
            Files.createDirectories(path.getParent());
        }
        Files.writeString(path, sb.toString(), StandardCharsets.UTF_8);
    }

    private static void writeBench(StringBuilder sb, BenchResult b, boolean comma) {
        sb.append("    {\n");
        field(sb, "name", b.name, true, 6);
        field(sb, "ops", b.ops, true, 6);
        field(sb, "elapsed_ns", b.elapsedNs, true, 6);
        field(sb, "throughput_ops_s", b.throughputOpsS, true, 6);
        field(sb, "checksum", b.checksum, true, 6);
        sb.append("      \"latency_ns\": {\n");
        field(sb, "min", b.minNs, true, 8);
        field(sb, "p50", b.p50Ns, true, 8);
        field(sb, "p90", b.p90Ns, true, 8);
        field(sb, "p99", b.p99Ns, true, 8);
        field(sb, "p999", b.p999Ns, true, 8);
        field(sb, "max", b.maxNs, false, 8);
        sb.append("      },\n");
        sb.append("      \"stats\": {\n");
        int n = 0;
        for (Map.Entry<String, Long> e : b.stats.entrySet()) {
            n++;
            field(sb, e.getKey(), e.getValue(), n < b.stats.size(), 8);
        }
        sb.append("      }\n");
        sb.append("    }").append(comma ? ",\n" : "\n");
    }

    private static void field(StringBuilder sb, String k, String v, boolean comma) {
        field(sb, k, v, comma, 2);
    }

    private static void field(StringBuilder sb, String k, long v, boolean comma) {
        field(sb, k, v, comma, 2);
    }

    private static void field(StringBuilder sb, String k, String v, boolean comma, int indent) {
        sb.append(" ".repeat(indent));
        sb.append("\"").append(k).append("\": \"").append(escape(v)).append("\"");
        sb.append(comma ? ",\n" : "\n");
    }

    private static void field(StringBuilder sb, String k, long v, boolean comma, int indent) {
        sb.append(" ".repeat(indent));
        sb.append("\"").append(k).append("\": ").append(v);
        sb.append(comma ? ",\n" : "\n");
    }

    private static void field(StringBuilder sb, String k, double v, boolean comma, int indent) {
        sb.append(" ".repeat(indent));
        sb.append("\"").append(k).append("\": ").append(String.format(Locale.US, "%.6f", v));
        sb.append(comma ? ",\n" : "\n");
    }

    private static String escape(String s) {
        return s.replace("\\", "\\\\").replace("\"", "\\\"");
    }
}
