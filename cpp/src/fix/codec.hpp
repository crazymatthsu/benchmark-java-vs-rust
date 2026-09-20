#pragma once

#include "../constants.hpp"
#include "../rng.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

inline std::string pad2(int32_t n) {
    if (n < 10) {
        return "0" + std::to_string(n);
    }
    return std::to_string(n);
}

inline std::string pad3(uint32_t n) {
    if (n < 10) {
        return "00" + std::to_string(n);
    }
    if (n < 100) {
        return "0" + std::to_string(n);
    }
    return std::to_string(n);
}

inline std::string build_new_order_single(
    int32_t seq, uint64_t cl_ord_id, int32_t symbol, int32_t side, int64_t qty, int32_t price) {
    std::string body;
    body += "35=D\x01";
    body += "34=" + std::to_string(seq) + "\x01";
    body += "49=SENDER\x01";
    body += "56=TARGET\x01";
    body += "11=" + std::to_string(cl_ord_id) + "\x01";
    body += "55=SYM" + pad2(symbol) + "\x01";
    body += std::string("54=") + (side == c::BUY ? "1" : "2") + "\x01";
    body += "38=" + std::to_string(qty) + "\x01";
    body += "44=" + std::to_string(price) + "\x01";
    body += "40=2\x01";
    body += "59=0\x01";
    std::string header = "8=FIX.4.4\x019=" + std::to_string(body.size()) + "\x01";
    std::string prefix = header + body;
    uint32_t sum = 0;
    for (unsigned char ch : prefix) {
        sum += ch;
    }
    return prefix + "10=" + pad3(sum % 256) + "\x01";
}

inline int64_t parse_long(const uint8_t* b, std::size_t s, std::size_t e) {
    int64_t v = 0;
    for (std::size_t i = s; i < e; i++) {
        uint8_t ch = b[i];
        if (ch >= '0' && ch <= '9') {
            v = v * 10 + static_cast<int64_t>(ch - '0');
        }
    }
    return v;
}

inline int32_t parse_symbol(const uint8_t* b, std::size_t s, std::size_t e) {
    std::size_t k = s;
    if (e - s >= 3 && b[k] == 'S' && b[k + 1] == 'Y' && b[k + 2] == 'M') {
        k += 3;
    }
    return static_cast<int32_t>(parse_long(b, k, e));
}

struct FixBench {
    std::vector<uint8_t> arena;
    std::vector<std::size_t> off;
    std::vector<std::size_t> len;
    uint64_t checksum = 0;

    FixBench(uint64_t seed, std::size_t n_msgs, int32_t n_symbols) {
        std::vector<std::string> tmp;
        tmp.reserve(n_msgs);
        XorShift64 rng(seed);
        std::size_t total = 0;
        for (std::size_t i = 0; i < n_msgs; i++) {
            uint64_t cl_ord_id = static_cast<uint64_t>(i) + 1;
            int32_t symbol = static_cast<int32_t>(rng.next_bounded(static_cast<uint32_t>(n_symbols)));
            int32_t side = static_cast<int32_t>(rng.next_bounded(2));
            int64_t qty = 1 + static_cast<int64_t>(rng.next_bounded(100));
            int32_t price = c::PRICE_MID + static_cast<int32_t>(rng.next_bounded(static_cast<uint32_t>(c::PRICE_SPAN)))
                            - c::PRICE_SPAN / 2;
            tmp.push_back(build_new_order_single(static_cast<int32_t>(i) + 1, cl_ord_id, symbol, side, qty, price));
            total += tmp.back().size();
        }
        arena.resize(total);
        off.resize(n_msgs);
        len.resize(n_msgs);
        std::size_t cursor = 0;
        for (std::size_t i = 0; i < n_msgs; i++) {
            off[i] = cursor;
            len[i] = tmp[i].size();
            std::copy(tmp[i].begin(), tmp[i].end(), arena.begin() + static_cast<std::ptrdiff_t>(cursor));
            cursor += tmp[i].size();
        }
    }

    void parse(std::size_t i) {
        std::size_t start = off[i];
        std::size_t end = start + len[i];
        int64_t cl_ord_id = 0;
        int32_t symbol = 0;
        int32_t side = 0;
        int64_t qty = 0;
        int64_t price = 0;
        std::size_t p = start;
        const uint8_t* a = arena.data();
        while (p < end) {
            int32_t tag = 0;
            while (p < end && a[p] != '=') {
                uint8_t ch = a[p];
                if (ch >= '0' && ch <= '9') {
                    tag = tag * 10 + static_cast<int32_t>(ch - '0');
                }
                p++;
            }
            if (p < end && a[p] == '=') {
                p++;
            }
            std::size_t vs = p;
            while (p < end && a[p] != 1) {
                p++;
            }
            std::size_t ve = p;
            if (p < end && a[p] == 1) {
                p++;
            }
            switch (tag) {
                case 11:
                    cl_ord_id = parse_long(a, vs, ve);
                    break;
                case 55:
                    symbol = parse_symbol(a, vs, ve);
                    break;
                case 54: {
                    int64_t v = parse_long(a, vs, ve);
                    side = v == 1 ? c::BUY : c::SELL;
                    break;
                }
                case 38:
                    qty = parse_long(a, vs, ve);
                    break;
                case 44:
                    price = parse_long(a, vs, ve);
                    break;
                default:
                    break;
            }
        }
        checksum = c::mix_i64(checksum, cl_ord_id);
        checksum = c::mix_i32(checksum, symbol);
        checksum = c::mix_i32(checksum, side);
        checksum = c::mix_i64(checksum, qty);
        checksum = c::mix_i64(checksum, price);
    }
};
