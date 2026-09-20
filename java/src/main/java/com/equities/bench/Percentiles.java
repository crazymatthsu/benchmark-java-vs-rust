package com.equities.bench;

import java.util.Arrays;

public final class Percentiles {
    private Percentiles() {}

    public static void sortInPlace(long[] samples) {
        Arrays.sort(samples);
    }

    public static long pct(long[] sorted, double p) {
        if (sorted.length == 0) {
            return 0;
        }
        int i = (int) Math.floor(p * (sorted.length - 1));
        if (i < 0) {
            i = 0;
        }
        if (i >= sorted.length) {
            i = sorted.length - 1;
        }
        return sorted[i];
    }

    public static long min(long[] sorted) {
        return sorted.length == 0 ? 0 : sorted[0];
    }

    public static long max(long[] sorted) {
        return sorted.length == 0 ? 0 : sorted[sorted.length - 1];
    }
}
