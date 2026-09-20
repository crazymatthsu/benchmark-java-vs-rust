package com.equities.bench;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertNotEquals;

import org.junit.jupiter.api.Test;

class XorShift64Test {
    @Test
    void sameSeedSameSequence() {
        XorShift64 a = new XorShift64(C.DEFAULT_SEED);
        XorShift64 b = new XorShift64(C.DEFAULT_SEED);
        for (int i = 0; i < 100; i++) {
            assertEquals(a.nextU64(), b.nextU64());
        }
    }

    @Test
    void zeroSeedUsesDefault() {
        XorShift64 a = new XorShift64(0);
        XorShift64 b = new XorShift64(C.DEFAULT_SEED);
        assertEquals(a.nextU64(), b.nextU64());
        assertNotEquals(0L, a.nextU64());
    }
}
