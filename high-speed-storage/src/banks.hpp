#pragma once

#include "mphf.hpp"

#include <stdexcept>

// Dual hashing banks (patent Fig. 6–7): the active bank is read-only for
// lookups; software programs the inactive bank, then a hitless switch
// (double buffer) makes it active between messages.

class DualBank {
public:
    HashBank bank[2];
    int active = 0;

    HashBank& current() { return bank[active]; }
    const HashBank& current() const { return bank[active]; }
    HashBank& inactive() { return bank[active ^ 1]; }

    uint32_t lookup(uint64_t packed) const { return current().lookup(packed); }

    void activate_inactive() { active ^= 1; }

    void rebuild_inactive(const InstrumentTable& table, uint64_t seed) {
        if (!build_mphf(inactive(), table, seed)) {
            throw std::runtime_error("failed to find a perfect hash (retry with more table slack)");
        }
        verify_perfect(inactive(), table);
    }
};
