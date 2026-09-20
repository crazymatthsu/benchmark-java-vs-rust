package com.equities.bench;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertTrue;

import org.junit.jupiter.api.Test;

class OrderBookEngineTest {
    @Test
    void matchingBuyHitsRestingSell() {
        OrderBookEngine eng = new OrderBookEngine(1, 8);
        eng.add(1, 0, C.SELL, 10_000, 10, C.TIF_GTC);
        eng.add(2, 0, C.BUY, 10_000, 10, C.TIF_GTC);
        assertEquals(1, eng.fillCount);
        assertEquals(10, eng.fillQty);
        assertEquals(0, eng.orders[1].qty);
        assertEquals(0, eng.orders[2].qty);
    }

    @Test
    void cancelRemovesLiquidity() {
        OrderBookEngine eng = new OrderBookEngine(1, 8);
        eng.add(1, 0, C.SELL, 10_000, 5, C.TIF_GTC);
        eng.cancel(1);
        eng.add(2, 0, C.BUY, 10_000, 5, C.TIF_IOC);
        assertEquals(0, eng.fillCount);
        assertEquals(1, eng.cancelHits);
        assertEquals(5, eng.iocKilled);
    }

    @Test
    void sameWorkloadIsDeterministic() {
        Workload w = Workload.generate(C.DEFAULT_SEED, 5_000, 8, 16);
        OrderBookEngine a = new OrderBookEngine(8, 5_000);
        OrderBookEngine b = new OrderBookEngine(8, 5_000);
        for (int i = 0; i < w.nOps; i++) {
            a.apply(w, i);
            b.apply(w, i);
        }
        a.finish();
        b.finish();
        assertEquals(a.checksum, b.checksum);
        assertEquals(a.fillCount, b.fillCount);
        assertTrue(a.fillCount > 0);
    }

    @Test
    void marketDataMatchesBinaryMdChecksum() {
        MarketDataBench md = new MarketDataBench(C.DEFAULT_SEED, 2_000, 8);
        BinaryMdBench bin = new BinaryMdBench(C.DEFAULT_SEED, 2_000, 8);
        for (int i = 0; i < 2_000; i++) {
            md.apply(i);
            bin.apply(i);
        }
        md.finish();
        bin.finish();
        assertEquals(md.checksum, bin.checksum);
    }
}
