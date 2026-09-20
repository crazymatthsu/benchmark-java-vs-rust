package com.equities.bench;

public final class TickAgg {
    public long open;
    public long high = Long.MIN_VALUE;
    public long low = Long.MAX_VALUE;
    public long close;
    public long notional;
    public long volume;
    public long ticks;
    public long bars;

    public void reset() {
        open = 0;
        high = Long.MIN_VALUE;
        low = Long.MAX_VALUE;
        close = 0;
        notional = 0;
        volume = 0;
        ticks = 0;
        bars = 0;
    }

    public void onTick(long px, long qty) {
        if (ticks == 0) {
            open = px;
        }
        if (px > high) {
            high = px;
        }
        if (px < low) {
            low = px;
        }
        close = px;
        notional += px * qty;
        volume += qty;
        ticks++;
        if (ticks % 100 == 0) {
            bars++;
        }
    }

    public long mix(long checksum) {
        checksum = checksum * C.MIX + open;
        checksum = checksum * C.MIX + high;
        checksum = checksum * C.MIX + low;
        checksum = checksum * C.MIX + close;
        checksum = checksum * C.MIX + notional;
        checksum = checksum * C.MIX + volume;
        checksum = checksum * C.MIX + bars;
        return checksum;
    }
}
