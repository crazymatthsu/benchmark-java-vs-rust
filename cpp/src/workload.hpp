#pragma once

#include "constants.hpp"
#include "rng.hpp"

#include <cstdint>
#include <vector>

struct Workload {
    std::vector<uint8_t> op;
    std::vector<int32_t> order_id;
    std::vector<int32_t> symbol;
    std::vector<int32_t> side;
    std::vector<int32_t> price;
    std::vector<int64_t> qty;
    std::vector<uint8_t> tif;
    std::vector<int32_t> account;
    std::size_t n_ops = 0;

    static Workload generate(uint64_t seed, std::size_t n_ops, int32_t n_symbols, int32_t n_accounts) {
        Workload w;
        w.n_ops = n_ops;
        w.op.resize(n_ops);
        w.order_id.resize(n_ops);
        w.symbol.resize(n_ops);
        w.side.resize(n_ops);
        w.price.resize(n_ops);
        w.qty.resize(n_ops);
        w.tif.resize(n_ops);
        w.account.resize(n_ops);
        XorShift64 rng(seed);
        int32_t next_id = 1;
        for (std::size_t i = 0; i < n_ops; i++) {
            uint32_t r = rng.next_bounded(100);
            if (r < 70 || next_id == 1) {
                w.op[i] = c::OP_ADD;
                w.order_id[i] = next_id++;
                w.symbol[i] = static_cast<int32_t>(rng.next_bounded(static_cast<uint32_t>(n_symbols)));
                w.side[i] = static_cast<int32_t>(rng.next_bounded(2));
                w.price[i] = c::PRICE_MID + static_cast<int32_t>(rng.next_bounded(static_cast<uint32_t>(c::PRICE_SPAN)))
                             - c::PRICE_SPAN / 2;
                w.qty[i] = 1 + static_cast<int64_t>(rng.next_bounded(100));
                w.tif[i] = rng.next_bounded(10) == 0 ? c::TIF_IOC : c::TIF_GTC;
                w.account[i] = static_cast<int32_t>(rng.next_bounded(static_cast<uint32_t>(n_accounts)));
            } else if (r < 90) {
                w.op[i] = c::OP_CANCEL;
                w.order_id[i] = 1 + static_cast<int32_t>(rng.next_bounded(static_cast<uint32_t>(next_id - 1)));
                w.account[i] = static_cast<int32_t>(rng.next_bounded(static_cast<uint32_t>(n_accounts)));
            } else {
                w.op[i] = c::OP_MARKET;
                w.order_id[i] = next_id++;
                w.symbol[i] = static_cast<int32_t>(rng.next_bounded(static_cast<uint32_t>(n_symbols)));
                w.side[i] = static_cast<int32_t>(rng.next_bounded(2));
                w.price[i] = 0;
                w.qty[i] = 1 + static_cast<int64_t>(rng.next_bounded(100));
                w.tif[i] = c::TIF_IOC;
                w.account[i] = static_cast<int32_t>(rng.next_bounded(static_cast<uint32_t>(n_accounts)));
            }
        }
        return w;
    }
};
