#pragma once

#include <cstdint>

// Hardware-friendly vector hash from US 11,948,192:
// split a 64-bit symbol into four 16-bit sub-keys, multiply each by a
// coefficient (FPGA DSP), XOR the products. Modulo a power-of-two table
// size is a bit mask (shift / bit-select in the patent).

inline uint32_t vector_hash(const uint16_t a[4], uint64_t key) {
    const uint16_t k0 = static_cast<uint16_t>(key);
    const uint16_t k1 = static_cast<uint16_t>(key >> 16);
    const uint16_t k2 = static_cast<uint16_t>(key >> 32);
    const uint16_t k3 = static_cast<uint16_t>(key >> 48);
    const uint32_t p0 = static_cast<uint32_t>(a[0]) * k0;
    const uint32_t p1 = static_cast<uint32_t>(a[1]) * k1;
    const uint32_t p2 = static_cast<uint32_t>(a[2]) * k2;
    const uint32_t p3 = static_cast<uint32_t>(a[3]) * k3;
    uint32_t h = p0 ^ p1 ^ p2 ^ p3;
    h ^= static_cast<uint32_t>(key >> 32);
    h ^= h >> 16;
    return h;
}

inline uint32_t hash_addr(const uint16_t a[4], uint64_t key, uint32_t mask) {
    return vector_hash(a, key) & mask;
}

// Splitmix64 for coefficient search (offline generate only).
inline uint64_t splitmix64(uint64_t& state) {
    uint64_t z = (state += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

inline uint16_t odd_coeff(uint64_t& rng) {
    return static_cast<uint16_t>(splitmix64(rng) | 1U);
}
