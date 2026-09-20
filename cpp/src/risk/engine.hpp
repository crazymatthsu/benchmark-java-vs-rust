#pragma once

#include "../constants.hpp"
#include "../workload.hpp"

#include <cstdint>
#include <cstdlib>
#include <vector>

struct RiskEngine {
    std::vector<std::vector<int64_t>> position;
    std::vector<int64_t> acct_notional;
    std::vector<int64_t> last_px;
    uint64_t accepts = 0;
    uint64_t rejects = 0;
    uint64_t checksum = 0;

    RiskEngine(int32_t n_accounts, int32_t n_symbols)
        : position(static_cast<std::size_t>(n_accounts), std::vector<int64_t>(static_cast<std::size_t>(n_symbols), 0)),
          acct_notional(static_cast<std::size_t>(n_accounts), 0),
          last_px(static_cast<std::size_t>(n_symbols), static_cast<int64_t>(c::PRICE_MID)) {}

    void apply(const Workload& w, std::size_t i) {
        if (w.op[i] == c::OP_CANCEL) {
            return;
        }
        std::size_t acct = static_cast<std::size_t>(w.account[i]);
        std::size_t sym = static_cast<std::size_t>(w.symbol[i]);
        int32_t side = w.side[i];
        int64_t qty = w.qty[i];
        int64_t px = w.price[i] == 0 ? last_px[sym] : static_cast<int64_t>(w.price[i]);
        int64_t signed_qty = side == c::BUY ? qty : -qty;
        int64_t notional = px * qty;

        bool reject = qty <= 0 || qty > c::MAX_ORDER_QTY;
        if (!reject) {
            int64_t abs_pos = std::llabs(position[acct][sym] + signed_qty);
            if (abs_pos > c::MAX_POSITION) {
                reject = true;
            } else if (acct_notional[acct] + notional > c::MAX_NOTIONAL) {
                reject = true;
            } else if (last_px[sym] > 0
                       && std::llabs(px - last_px[sym]) * 10'000 > last_px[sym] * c::COLLAR_BPS) {
                reject = true;
            }
        }
        if (reject) {
            rejects++;
        } else {
            position[acct][sym] += signed_qty;
            acct_notional[acct] += notional;
            last_px[sym] = px;
            accepts++;
        }
    }

    void finish() {
        checksum = c::mix(checksum, accepts);
        checksum = c::mix(checksum, rejects);
        for (const auto& row : position) {
            for (int64_t p : row) {
                checksum = c::mix_i64(checksum, p);
            }
        }
    }
};
