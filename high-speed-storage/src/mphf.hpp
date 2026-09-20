#pragma once

#include "hash.hpp"
#include "instrument.hpp"

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

// Compress-hash-displace style perfect hash (software stand-in for the
// patent's G_Map + coefficient registers):
//   bucket = H0(key) & bucket_mask
//   slot   = (H1(key) + g[bucket]) & slot_mask
// g[] is programmed into mapping memory; H0/H1 are 4×16-bit vector hashes.

struct MapEntry {
    uint32_t word = 0;

    bool occupied() const { return (word >> 31) != 0; }
    uint32_t instrument() const { return word & 0x7FFFFFFFu; }
    void set(uint32_t instrument_id) { word = (1u << 31) | (instrument_id & 0x7FFFFFFFu); }
};

struct HashBank {
    uint16_t coeff_bucket[4]{};
    uint16_t coeff_slot[4]{};
    uint32_t bucket_mask = 0;
    uint32_t slot_mask = 0;
    uint32_t n_buckets = 0;
    uint32_t n_slots = 0;
    std::vector<uint32_t> g;      // G_Map: per-bucket displacement
    std::vector<MapEntry> slots;  // instrument index
    uint64_t seed_used = 0;

    uint32_t bucket_of(uint64_t packed) const {
        return hash_addr(coeff_bucket, packed, bucket_mask);
    }
    uint32_t slot_of(uint64_t packed, uint32_t disp) const {
        return (vector_hash(coeff_slot, packed) + disp) & slot_mask;
    }

    uint32_t lookup(uint64_t packed) const {
        const uint32_t b = bucket_of(packed);
        const uint32_t s = slot_of(packed, g[b]);
        const MapEntry& e = slots[s];
        if (!e.occupied()) {
            return UINT32_MAX;
        }
        return e.instrument();
    }
};

inline uint32_t next_pow2(uint32_t n) {
    uint32_t p = 1;
    while (p < n) {
        p <<= 1;
    }
    return p;
}

inline bool build_mphf(HashBank& bank, const InstrumentTable& table, uint64_t seed) {
    const uint32_t n = static_cast<uint32_t>(table.size());
    if (n == 0) {
        throw std::runtime_error("no symbols");
    }
    {
        std::vector<uint64_t> keys;
        keys.reserve(n);
        for (const auto& r : table.rows) {
            keys.push_back(r.packed);
        }
        std::sort(keys.begin(), keys.end());
        for (uint32_t i = 1; i < n; i++) {
            if (keys[i] == keys[i - 1]) {
                throw std::runtime_error("duplicate packed symbol in table");
            }
        }
    }
    const uint32_t n_buckets = next_pow2(std::max(8u, n * 2));
    const uint32_t n_slots = next_pow2(std::max(16u, n * 2));
    uint64_t rng = seed ? seed : 0xC0FFEE123456789ULL;

    for (int attempt = 0; attempt < 256; attempt++) {
        for (int c = 0; c < 4; c++) {
            bank.coeff_bucket[c] = odd_coeff(rng);
            bank.coeff_slot[c] = odd_coeff(rng);
        }
        bank.n_buckets = n_buckets;
        bank.n_slots = n_slots;
        bank.bucket_mask = n_buckets - 1;
        bank.slot_mask = n_slots - 1;
        bank.seed_used = rng;
        bank.g.assign(n_buckets, 0);
        bank.slots.assign(n_slots, MapEntry{});

        std::vector<std::vector<uint32_t>> buckets(n_buckets);
        for (uint32_t i = 0; i < n; i++) {
            buckets[hash_addr(bank.coeff_bucket, table.rows[i].packed, bank.bucket_mask)].push_back(i);
        }
        std::vector<uint32_t> order(n_buckets);
        for (uint32_t i = 0; i < n_buckets; i++) {
            order[i] = i;
        }
        std::sort(order.begin(), order.end(), [&](uint32_t a, uint32_t b) {
            return buckets[a].size() > buckets[b].size();
        });

        bool ok = true;
        for (uint32_t bi : order) {
            const auto& keys = buckets[bi];
            if (keys.empty()) {
                continue;
            }
            bool placed = false;
            for (uint32_t d = 0; d < n_slots; d++) {
                bool clash = false;
                std::vector<uint32_t> used;
                used.reserve(keys.size());
                for (uint32_t ki : keys) {
                    const uint32_t s = bank.slot_of(table.rows[ki].packed, d);
                    if (bank.slots[s].occupied()) {
                        clash = true;
                        break;
                    }
                    for (uint32_t u : used) {
                        if (u == s) {
                            clash = true;
                            break;
                        }
                    }
                    if (clash) {
                        break;
                    }
                    used.push_back(s);
                }
                if (clash) {
                    continue;
                }
                bank.g[bi] = d;
                for (uint32_t ki : keys) {
                    const uint32_t s = bank.slot_of(table.rows[ki].packed, d);
                    bank.slots[s].set(table.rows[ki].id);
                }
                placed = true;
                break;
            }
            if (!placed) {
                ok = false;
                break;
            }
        }
        if (ok) {
            return true;
        }
    }
    return false;
}

inline void verify_perfect(const HashBank& bank, const InstrumentTable& table) {
    for (const Instrument& ins : table.rows) {
        const uint32_t id = bank.lookup(ins.packed);
        if (id != ins.id) {
            throw std::runtime_error("lookup mismatch for " + ins.display);
        }
    }
}
