#pragma once

#include "../constants.hpp"

#include <cstdint>
#include <limits>

struct TickAgg {
    int64_t open = 0;
    int64_t high = std::numeric_limits<int64_t>::min();
    int64_t low = std::numeric_limits<int64_t>::max();
    int64_t close = 0;
    int64_t notional = 0;
    int64_t volume = 0;
    int64_t ticks = 0;
    int64_t bars = 0;

    void reset() { *this = TickAgg{}; }

    void on_tick(int64_t px, int64_t qty) {
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

    uint64_t mix(uint64_t checksum) const {
        checksum = c::mix_i64(checksum, open);
        checksum = c::mix_i64(checksum, high);
        checksum = c::mix_i64(checksum, low);
        checksum = c::mix_i64(checksum, close);
        checksum = c::mix_i64(checksum, notional);
        checksum = c::mix_i64(checksum, volume);
        checksum = c::mix_i64(checksum, bars);
        return checksum;
    }
};
