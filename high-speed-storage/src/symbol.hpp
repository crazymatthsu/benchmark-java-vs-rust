#pragma once

#include <cctype>
#include <cstdint>
#include <string>
#include <string_view>

// Pack an ALPHA ticker or INT id into a 64-bit word. Venue id is XOR'd into
// the high byte (patent: XOR venue id with an unused symbol byte). Tickers
// are at most 7 ASCII bytes so that byte is free.

inline uint64_t pack_alpha(std::string_view s) {
    uint64_t bits = 0;
    unsigned n = 0;
    for (unsigned char ch : s) {
        if (ch == ' ' || ch == '\t' || ch == '\r') {
            continue;
        }
        if (n >= 7) {
            break;
        }
        bits |= static_cast<uint64_t>(ch) << (8 * n);
        n++;
    }
    return bits;
}

inline uint64_t pack_int(std::string_view s) {
    uint64_t v = 0;
    for (unsigned char ch : s) {
        if (ch >= '0' && ch <= '9') {
            v = v * 10 + (ch - '0');
        }
    }
    return v;
}

inline uint64_t apply_venue(uint64_t packed, uint8_t venue_id) {
    return packed ^ (static_cast<uint64_t>(venue_id) << 56);
}

inline std::string unpack_alpha(uint64_t packed) {
    packed &= 0x00FFFFFFFFFFFFFFULL;
    std::string out;
    for (int i = 0; i < 7; i++) {
        char ch = static_cast<char>(packed & 0xFF);
        packed >>= 8;
        if (ch == 0) {
            break;
        }
        out.push_back(ch);
    }
    return out;
}

inline std::string trim(std::string_view s) {
    std::size_t a = 0;
    std::size_t b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) {
        a++;
    }
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) {
        b--;
    }
    return std::string(s.substr(a, b - a));
}
