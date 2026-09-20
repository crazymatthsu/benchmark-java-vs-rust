#pragma once

#include "instrument.hpp"
#include "mphf.hpp"
#include "symbol.hpp"

#include <cstdint>
#include <filesystem>

#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

struct SourceSpec {
    std::string filename;
    std::string type;  // ALPHA or INT
    uint8_t venue = 0;
};

inline std::vector<std::string> split_ws(const std::string& line) {
    std::vector<std::string> out;
    std::istringstream in(line);
    std::string tok;
    while (in >> tok) {
        out.push_back(tok);
    }
    return out;
}

inline std::vector<SourceSpec> load_exchanges(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("cannot open " + path.string());
    }
    std::vector<SourceSpec> specs;
    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }
        auto t = split_ws(line);
        if (t.size() < 4) {
            throw std::runtime_error("bad exchanges line: " + line);
        }
        SourceSpec s;
        s.filename = t[0];
        s.type = t[2];
        s.venue = static_cast<uint8_t>(std::stoi(t[3]));
        specs.push_back(s);
    }
    return specs;
}

inline void load_symbol_file(const std::filesystem::path& path, const SourceSpec& spec, InstrumentTable& table) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("cannot open " + path.string());
    }
    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }
        std::vector<std::string> aliases;
        std::string cur;
        for (char ch : line) {
            if (ch == '|') {
                aliases.push_back(trim(cur));
                cur.clear();
            } else {
                cur.push_back(ch);
            }
        }
        aliases.push_back(trim(cur));
        if (aliases.empty() || aliases[0].empty()) {
            continue;
        }
        // First token is canonical; remaining tokens are venue aliases that
        // hash to the same instrument id (mapping file).
        Instrument ins;
        ins.venue = spec.venue;
        ins.display = aliases[0];
        ins.last_px = 10'000 + static_cast<int32_t>(table.size() % 500);
        ins.easy_to_borrow = 1;
        if (spec.type == "INT") {
            ins.packed = apply_venue(pack_int(aliases[0]), spec.venue);
        } else {
            ins.packed = apply_venue(pack_alpha(aliases[0]), spec.venue);
        }
        uint32_t id;
        if (Instrument* existing = table.find_packed(ins.packed)) {
            id = existing->id;
        } else {
            id = table.add(ins);
        }
        for (std::size_t i = 1; i < aliases.size(); i++) {
            if (aliases[i].empty()) {
                continue;
            }
            Instrument alias;
            alias.venue = spec.venue;
            alias.display = aliases[i];
            alias.last_px = table.rows[id].last_px;
            alias.easy_to_borrow = table.rows[id].easy_to_borrow;
            alias.packed = apply_venue(pack_alpha(aliases[i]), spec.venue);
            if (table.find_packed(alias.packed)) {
                continue;
            }
            const uint32_t aid = table.add(alias);
            table.rows[aid].id = id;
        }
    }
}

inline InstrumentTable load_all(const std::filesystem::path& exchanges) {
    InstrumentTable table;
    const auto specs = load_exchanges(exchanges);
    const auto dir = exchanges.parent_path();
    for (const auto& spec : specs) {
        load_symbol_file(dir / spec.filename, spec, table);
    }
    return table;
}

inline void write_host_registers(const std::filesystem::path& path, const HashBank& bank) {
    std::ofstream out(path);
    out << "# register_name value   (host register file)\n";
    out << "BUCKET_MASK 0x" << std::hex << bank.bucket_mask << std::dec << "\n";
    out << "SLOT_MASK 0x" << std::hex << bank.slot_mask << std::dec << "\n";
    out << "N_BUCKETS " << bank.n_buckets << "\n";
    out << "N_SLOTS " << bank.n_slots << "\n";
    out << "SEED 0x" << std::hex << bank.seed_used << std::dec << "\n";
    for (int c = 0; c < 4; c++) {
        out << "A_BUCKET" << c << " 0x" << std::hex << bank.coeff_bucket[c] << std::dec << "\n";
        out << "A_SLOT" << c << " 0x" << std::hex << bank.coeff_slot[c] << std::dec << "\n";
    }
}

inline void write_host_dma(const std::filesystem::path& path, const HashBank& bank) {
    std::ofstream out(path);
    out << "# addr value   (host DMA file — G_Map displacements + slots)\n";
    for (uint32_t i = 0; i < bank.n_buckets; i++) {
        out << "g." << i << " " << bank.g[i] << "\n";
    }
    for (uint32_t i = 0; i < bank.n_slots; i++) {
        if (!bank.slots[i].occupied()) {
            continue;
        }
        out << std::hex << "slot." << i << " 0x" << bank.slots[i].word << std::dec << "\n";
    }
}

inline void write_hash_state(const std::filesystem::path& path, const HashBank& bank, const InstrumentTable& table) {
    std::ofstream out(path, std::ios::binary);
    const uint32_t magic = 0x48535331;  // 'HSS1'
    const uint32_t n = static_cast<uint32_t>(table.size());
    out.write(reinterpret_cast<const char*>(&magic), 4);
    out.write(reinterpret_cast<const char*>(&bank.seed_used), 8);
    out.write(reinterpret_cast<const char*>(&bank.n_buckets), 4);
    out.write(reinterpret_cast<const char*>(&bank.n_slots), 4);
    out.write(reinterpret_cast<const char*>(&n), 4);
    out.write(reinterpret_cast<const char*>(bank.coeff_bucket), sizeof(bank.coeff_bucket));
    out.write(reinterpret_cast<const char*>(bank.coeff_slot), sizeof(bank.coeff_slot));
    out.write(reinterpret_cast<const char*>(bank.g.data()),
              static_cast<std::streamsize>(bank.n_buckets * sizeof(uint32_t)));
    out.write(reinterpret_cast<const char*>(bank.slots.data()),
              static_cast<std::streamsize>(bank.n_slots * sizeof(MapEntry)));
    for (const auto& ins : table.rows) {
        out.write(reinterpret_cast<const char*>(&ins.id), 4);
        out.write(reinterpret_cast<const char*>(&ins.packed), 8);
        out.write(reinterpret_cast<const char*>(&ins.venue), 1);
        out.write(reinterpret_cast<const char*>(&ins.last_px), 4);
        out.write(reinterpret_cast<const char*>(&ins.halted), 1);
        out.write(reinterpret_cast<const char*>(&ins.easy_to_borrow), 1);
        const uint32_t len = static_cast<uint32_t>(ins.display.size());
        out.write(reinterpret_cast<const char*>(&len), 4);
        out.write(ins.display.data(), len);
    }
}

inline void read_hash_state(const std::filesystem::path& path, HashBank& bank, InstrumentTable& table) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("cannot open hash state " + path.string());
    }
    uint32_t magic = 0;
    uint32_t n = 0;
    in.read(reinterpret_cast<char*>(&magic), 4);
    if (magic != 0x48535331) {
        throw std::runtime_error("bad hash state magic");
    }
    in.read(reinterpret_cast<char*>(&bank.seed_used), 8);
    in.read(reinterpret_cast<char*>(&bank.n_buckets), 4);
    in.read(reinterpret_cast<char*>(&bank.n_slots), 4);
    in.read(reinterpret_cast<char*>(&n), 4);
    in.read(reinterpret_cast<char*>(bank.coeff_bucket), sizeof(bank.coeff_bucket));
    in.read(reinterpret_cast<char*>(bank.coeff_slot), sizeof(bank.coeff_slot));
    bank.bucket_mask = bank.n_buckets - 1;
    bank.slot_mask = bank.n_slots - 1;
    bank.g.resize(bank.n_buckets);
    bank.slots.resize(bank.n_slots);
    in.read(reinterpret_cast<char*>(bank.g.data()),
            static_cast<std::streamsize>(bank.n_buckets * sizeof(uint32_t)));
    in.read(reinterpret_cast<char*>(bank.slots.data()),
            static_cast<std::streamsize>(bank.n_slots * sizeof(MapEntry)));
    table.rows.clear();
    for (uint32_t i = 0; i < n; i++) {
        Instrument ins;
        uint32_t len = 0;
        in.read(reinterpret_cast<char*>(&ins.id), 4);
        in.read(reinterpret_cast<char*>(&ins.packed), 8);
        in.read(reinterpret_cast<char*>(&ins.venue), 1);
        in.read(reinterpret_cast<char*>(&ins.last_px), 4);
        in.read(reinterpret_cast<char*>(&ins.halted), 1);
        in.read(reinterpret_cast<char*>(&ins.easy_to_borrow), 1);
        in.read(reinterpret_cast<char*>(&len), 4);
        ins.display.resize(len);
        in.read(ins.display.data(), len);
        table.rows.push_back(std::move(ins));
    }
}
