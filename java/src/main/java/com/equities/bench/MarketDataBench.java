package com.equities.bench;

public final class MarketDataBench {
    public final int[] symbol;
    public final int[] price;
    public final long[] qty;
    public final TickAgg[] aggs;
    public long checksum;

    public MarketDataBench(long seed, int nTicks, int nSymbols) {
        symbol = new int[nTicks];
        price = new int[nTicks];
        qty = new long[nTicks];
        aggs = new TickAgg[nSymbols];
        for (int i = 0; i < nSymbols; i++) {
            aggs[i] = new TickAgg();
        }
        XorShift64 rng = new XorShift64(seed);
        for (int i = 0; i < nTicks; i++) {
            symbol[i] = rng.nextBounded(nSymbols);
            price[i] = C.PRICE_MID + rng.nextBounded(C.PRICE_SPAN) - C.PRICE_SPAN / 2;
            qty[i] = 1 + rng.nextBounded(100);
        }
    }

    public void apply(int i) {
        aggs[symbol[i]].onTick(price[i], qty[i]);
    }

    public void finish() {
        for (TickAgg a : aggs) {
            checksum = a.mix(checksum);
        }
    }
}
