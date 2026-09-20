#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct Instrument {
    uint32_t id = 0;
    uint64_t packed = 0;
    uint8_t venue = 0;
    int32_t last_px = 0;      // integer ticks
    uint8_t halted = 0;
    uint8_t easy_to_borrow = 1;
    std::string display;
};

class InstrumentTable {
public:
    std::vector<Instrument> rows;

    uint32_t add(Instrument ins) {
        const uint32_t id = static_cast<uint32_t>(rows.size());
        ins.id = id;
        rows.push_back(std::move(ins));
        return id;
    }

    Instrument* find_packed(uint64_t packed) {
        for (auto& r : rows) {
            if (r.packed == packed) {
                return &r;
            }
        }
        return nullptr;
    }

    Instrument* find_display(const std::string& display, uint8_t venue) {
        for (auto& r : rows) {
            if (r.display == display && r.venue == venue) {
                return &r;
            }
        }
        return nullptr;
    }

    std::size_t size() const { return rows.size(); }
};
