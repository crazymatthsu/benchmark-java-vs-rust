package com.equities.bench;

public final class XorShift64 {
    private long state;

    public XorShift64(long seed) {
        this.state = seed == 0 ? C.DEFAULT_SEED : seed;
    }

    public long nextU64() {
        long x = state;
        x ^= x << 13;
        x ^= x >>> 7;
        x ^= x << 17;
        state = x;
        return x;
    }

    public int nextBounded(int n) {
        if (n <= 0) {
            throw new IllegalArgumentException("n");
        }
        return (int) Long.remainderUnsigned(nextU64(), n);
    }
}
