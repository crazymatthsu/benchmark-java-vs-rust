#pragma once

#include "constants.hpp"

#include <cstdint>

struct XorShift64 {
    uint64_t state;

    explicit XorShift64(uint64_t seed)
        : state(seed == 0 ? c::DEFAULT_SEED : seed) {}

    uint64_t next_u64() {
        uint64_t x = state;
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
        state = x;
        return x;
    }

    uint32_t next_bounded(uint32_t n) {
        return static_cast<uint32_t>(next_u64() % static_cast<uint64_t>(n));
    }
};
