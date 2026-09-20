#pragma once

#include "../constants.hpp"
#include "../rng.hpp"
#include "agg.hpp"

#include <cstdint>
#include <cstring>
#include <vector>

inline constexpr std::size_t REC = 32;

inline void write_u32(uint8_t* a, std::size_t o, uint32_t v) {
    a[o] = static_cast<uint8_t>(v);
    a[o + 1] = static_cast<uint8_t>(v >> 8);
    a[o + 2] = static_cast<uint8_t>(v >> 16);
    a[o + 3] = static_cast<uint8_t>(v >> 24);
}

inline void write_i64(uint8_t* a, std::size_t o, int64_t v) {
    uint64_t u = static_cast<uint64_t>(v);
    a[o] = static_cast<uint8_t>(u);
    a[o + 1] = static_cast<uint8_t>(u >> 8);
    a[o + 2] = static_cast<uint8_t>(u >> 16);
    a[o + 3] = static_cast<uint8_t>(u >> 24);
    a[o + 4] = static_cast<uint8_t>(u >> 32);
    a[o + 5] = static_cast<uint8_t>(u >> 40);
    a[o + 6] = static_cast<uint8_t>(u >> 48);
    a[o + 7] = static_cast<uint8_t>(u >> 56);
}

inline uint32_t read_u32(const uint8_t* a, std::size_t o) {
    return static_cast<uint32_t>(a[o])
           | (static_cast<uint32_t>(a[o + 1]) << 8)
           | (static_cast<uint32_t>(a[o + 2]) << 16)
           | (static_cast<uint32_t>(a[o + 3]) << 24);
}

inline int64_t read_i64(const uint8_t* a, std::size_t o) {
    uint64_t u = static_cast<uint64_t>(a[o])
                 | (static_cast<uint64_t>(a[o + 1]) << 8)
                 | (static_cast<uint64_t>(a[o + 2]) << 16)
                 | (static_cast<uint64_t>(a[o + 3]) << 24)
                 | (static_cast<uint64_t>(a[o + 4]) << 32)
                 | (static_cast<uint64_t>(a[o + 5]) << 40)
                 | (static_cast<uint64_t>(a[o + 6]) << 48)
                 | (static_cast<uint64_t>(a[o + 7]) << 56);
    return static_cast<int64_t>(u);
}

struct BinaryMdBench {
    std::vector<uint8_t> arena;
    std::vector<TickAgg> aggs;
    uint64_t checksum = 0;

    BinaryMdBench(uint64_t seed, std::size_t n_ticks, int32_t n_symbols)
        : arena(n_ticks * REC), aggs(static_cast<std::size_t>(n_symbols)) {
        XorShift64 rng(seed);
        std::size_t o = 0;
        for (std::size_t i = 0; i < n_ticks; i++) {
            int32_t symbol = static_cast<int32_t>(rng.next_bounded(static_cast<uint32_t>(n_symbols)));
            int32_t price = c::PRICE_MID + static_cast<int32_t>(rng.next_bounded(static_cast<uint32_t>(c::PRICE_SPAN)))
                            - c::PRICE_SPAN / 2;
            int64_t qty = 1 + static_cast<int64_t>(rng.next_bounded(100));
            write_u32(arena.data(), o, static_cast<uint32_t>(symbol));
            write_u32(arena.data(), o + 4, 0);
            write_i64(arena.data(), o + 8, price);
            write_i64(arena.data(), o + 16, qty);
            write_i64(arena.data(), o + 24, static_cast<int64_t>(i));
            o += REC;
        }
    }

    void apply(std::size_t i) {
        std::size_t o = i * REC;
        uint32_t symbol = read_u32(arena.data(), o);
        int64_t price = read_i64(arena.data(), o + 8);
        int64_t qty = read_i64(arena.data(), o + 16);
        aggs[symbol].on_tick(price, qty);
    }

    void reset_aggs() {
        for (auto& a : aggs) {
            a.reset();
        }
        checksum = 0;
    }

    void finish() {
        for (const auto& a : aggs) {
            checksum = a.mix(checksum);
        }
    }
};
