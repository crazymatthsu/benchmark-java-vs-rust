#pragma once

#include <cstdint>

namespace c {

inline constexpr int32_t PRICE_MIN = 1;
inline constexpr int32_t PRICE_MAX = 20'000;
inline constexpr int32_t PRICE_MID = 10'000;
inline constexpr int32_t PRICE_SPAN = 200;
inline constexpr uint64_t MIX = 1'000'003;
inline constexpr uint64_t DEFAULT_SEED = 0x0C0FFEE123456789ULL;

inline constexpr int32_t BUY = 0;
inline constexpr int32_t SELL = 1;
inline constexpr uint8_t TIF_GTC = 0;
inline constexpr uint8_t TIF_IOC = 1;
inline constexpr uint8_t OP_ADD = 0;
inline constexpr uint8_t OP_CANCEL = 1;
inline constexpr uint8_t OP_MARKET = 2;

inline constexpr int64_t MAX_ORDER_QTY = 10'000;
inline constexpr int64_t MAX_POSITION = 50'000;
inline constexpr int64_t MAX_NOTIONAL = 1'000'000'000'000LL;
inline constexpr int64_t COLLAR_BPS = 200;

inline uint64_t mix(uint64_t checksum, uint64_t x) {
    return checksum * MIX + x;
}

inline uint64_t mix_i64(uint64_t checksum, int64_t x) {
    return mix(checksum, static_cast<uint64_t>(x));
}

inline uint64_t mix_i32(uint64_t checksum, int32_t x) {
    return mix(checksum, static_cast<uint64_t>(x));
}

}  // namespace c
