#pragma once

#include "../constants.hpp"
#include "../rng.hpp"
#include "agg.hpp"

#include <cstdint>
#include <vector>

struct MarketDataBench {
    std::vector<int32_t> symbol;
    std::vector<int32_t> price;
    std::vector<int64_t> qty;
    std::vector<TickAgg> aggs;
    uint64_t checksum = 0;

    MarketDataBench(uint64_t seed, std::size_t n_ticks, int32_t n_symbols)
        : symbol(n_ticks), price(n_ticks), qty(n_ticks), aggs(static_cast<std::size_t>(n_symbols)) {
        XorShift64 rng(seed);
        for (std::size_t i = 0; i < n_ticks; i++) {
            symbol[i] = static_cast<int32_t>(rng.next_bounded(static_cast<uint32_t>(n_symbols)));
            price[i] = c::PRICE_MID + static_cast<int32_t>(rng.next_bounded(static_cast<uint32_t>(c::PRICE_SPAN)))
                       - c::PRICE_SPAN / 2;
            qty[i] = 1 + static_cast<int64_t>(rng.next_bounded(100));
        }
    }

    void apply(std::size_t i) { aggs[static_cast<std::size_t>(symbol[i])].on_tick(price[i], qty[i]); }

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
