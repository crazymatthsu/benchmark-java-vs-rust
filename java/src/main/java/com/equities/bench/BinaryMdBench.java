package com.equities.bench;

public final class BinaryMdBench {
    public static final int REC = 32;
    public final byte[] arena;
    public final TickAgg[] aggs;
    public final int nTicks;
    public long checksum;

    public BinaryMdBench(long seed, int nTicks, int nSymbols) {
        this.nTicks = nTicks;
        this.arena = new byte[nTicks * REC];
        this.aggs = new TickAgg[nSymbols];
        for (int i = 0; i < nSymbols; i++) {
            aggs[i] = new TickAgg();
        }
        XorShift64 rng = new XorShift64(seed);
        int o = 0;
        for (int i = 0; i < nTicks; i++) {
            int symbol = rng.nextBounded(nSymbols);
            int price = C.PRICE_MID + rng.nextBounded(C.PRICE_SPAN) - C.PRICE_SPAN / 2;
            long qty = 1 + rng.nextBounded(100);
            writeU32(arena, o, symbol);
            writeU32(arena, o + 4, 0);
            writeI64(arena, o + 8, price);
            writeI64(arena, o + 16, qty);
            writeI64(arena, o + 24, i);
            o += REC;
        }
    }

    public void apply(int i) {
        int o = i * REC;
        int symbol = readU32(arena, o);
        long price = readI64(arena, o + 8);
        long qty = readI64(arena, o + 16);
        aggs[symbol].onTick(price, qty);
    }

    public void finish() {
        for (TickAgg a : aggs) {
            checksum = a.mix(checksum);
        }
    }

    static void writeU32(byte[] a, int o, int v) {
        a[o] = (byte) v;
        a[o + 1] = (byte) (v >>> 8);
        a[o + 2] = (byte) (v >>> 16);
        a[o + 3] = (byte) (v >>> 24);
    }

    static void writeI64(byte[] a, int o, long v) {
        a[o] = (byte) v;
        a[o + 1] = (byte) (v >>> 8);
        a[o + 2] = (byte) (v >>> 16);
        a[o + 3] = (byte) (v >>> 24);
        a[o + 4] = (byte) (v >>> 32);
        a[o + 5] = (byte) (v >>> 40);
        a[o + 6] = (byte) (v >>> 48);
        a[o + 7] = (byte) (v >>> 56);
    }

    static int readU32(byte[] a, int o) {
        return (a[o] & 0xff)
                | ((a[o + 1] & 0xff) << 8)
                | ((a[o + 2] & 0xff) << 16)
                | ((a[o + 3] & 0xff) << 24);
    }

    static long readI64(byte[] a, int o) {
        return (long) (a[o] & 0xff)
                | ((long) (a[o + 1] & 0xff) << 8)
                | ((long) (a[o + 2] & 0xff) << 16)
                | ((long) (a[o + 3] & 0xff) << 24)
                | ((long) (a[o + 4] & 0xff) << 32)
                | ((long) (a[o + 5] & 0xff) << 40)
                | ((long) (a[o + 6] & 0xff) << 48)
                | ((long) (a[o + 7] & 0xff) << 56);
    }
}
