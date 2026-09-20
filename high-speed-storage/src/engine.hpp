#pragma once

#include "banks.hpp"
#include "files.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

class Engine {
public:
    DualBank banks;
    InstrumentTable table;

    void generate_from(const std::filesystem::path& exchanges, uint64_t seed = 1) {
        table = load_all(exchanges);
        banks.rebuild_inactive(table, seed);
        banks.activate_inactive();
    }

    void update_from(const std::filesystem::path& exchanges, uint64_t seed = 2) {
        // Intraday: rebuild hash coefficients + G_Map on the inactive bank,
        // then switch. Lookups on the active bank are uninterrupted until
        // the swap (patent double-buffer / hitless switch).
        table = load_all(exchanges);
        banks.rebuild_inactive(table, seed);
        banks.activate_inactive();
    }

    const Instrument* lookup_packed(uint64_t packed) const {
        const uint32_t id = banks.lookup(packed);
        if (id == UINT32_MAX || id >= table.rows.size()) {
            return nullptr;
        }
        return &table.rows[id];
    }

    const Instrument* lookup_symbol(std::string_view raw, uint8_t venue, bool integer = false) const {
        const uint64_t packed = apply_venue(integer ? pack_int(raw) : pack_alpha(raw), venue);
        return lookup_packed(packed);
    }

    void write_outputs(const std::filesystem::path& dir) const {
        std::filesystem::create_directories(dir);
        write_host_registers(dir / "host_registers.txt", banks.current());
        write_host_dma(dir / "host_dma.txt", banks.current());
        write_hash_state(dir / "hash_state.bin", banks.current(), table);
        std::ofstream sim(dir / "sim_registers.txt");
        sim << "# simulation register preload\n";
        sim << "slot_mask=" << banks.current().slot_mask << "\n";
        sim << "n_symbols=" << table.size() << "\n";
        sim << "active_bank=" << banks.active << "\n";
    }

    void load_state(const std::filesystem::path& dir) {
        read_hash_state(dir / "hash_state.bin", banks.current(), table);
        verify_perfect(banks.current(), table);
    }

    void dump_summary(std::ostream& os) const {
        const HashBank& b = banks.current();
        os << "symbols=" << table.size()
           << " buckets=" << b.n_buckets
           << " slots=" << b.n_slots
           << " active_bank=" << banks.active
           << " seed=0x" << std::hex << b.seed_used << std::dec << "\n";
    }
};
