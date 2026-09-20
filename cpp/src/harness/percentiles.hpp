#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

inline void sort_in_place(std::vector<uint64_t>& samples) {
    std::sort(samples.begin(), samples.end());
}

inline uint64_t pct(const std::vector<uint64_t>& sorted, double p) {
    if (sorted.empty()) {
        return 0;
    }
    auto i = static_cast<std::size_t>(std::floor(p * static_cast<double>(sorted.size() - 1)));
    if (i >= sorted.size()) {
        i = sorted.size() - 1;
    }
    return sorted[i];
}

inline uint64_t min_v(const std::vector<uint64_t>& sorted) {
    return sorted.empty() ? 0 : sorted.front();
}

inline uint64_t max_v(const std::vector<uint64_t>& sorted) {
    return sorted.empty() ? 0 : sorted.back();
}
