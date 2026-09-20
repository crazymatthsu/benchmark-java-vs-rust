package com.equities.bench;

import java.util.LinkedHashMap;
import java.util.Map;

public final class BenchResult {
    public final String name;
    public final int ops;
    public final long elapsedNs;
    public final double throughputOpsS;
    public final String checksum;
    public final long minNs;
    public final long p50Ns;
    public final long p90Ns;
    public final long p99Ns;
    public final long p999Ns;
    public final long maxNs;
    public final Map<String, Long> stats = new LinkedHashMap<>();

    public BenchResult(
            String name,
            int ops,
            long elapsedNs,
            long checksumBits,
            long[] sortedSamples) {
        this.name = name;
        this.ops = ops;
        this.elapsedNs = elapsedNs;
        this.throughputOpsS = elapsedNs <= 0 ? 0.0 : ops / (elapsedNs / 1_000_000_000.0);
        this.checksum = Long.toUnsignedString(checksumBits);
        this.minNs = Percentiles.min(sortedSamples);
        this.p50Ns = Percentiles.pct(sortedSamples, 0.50);
        this.p90Ns = Percentiles.pct(sortedSamples, 0.90);
        this.p99Ns = Percentiles.pct(sortedSamples, 0.99);
        this.p999Ns = Percentiles.pct(sortedSamples, 0.999);
        this.maxNs = Percentiles.max(sortedSamples);
    }

    public BenchResult stat(String key, long value) {
        stats.put(key, value);
        return this;
    }
}
