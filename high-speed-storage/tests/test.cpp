#include "../src/engine.hpp"
#include "../src/symbol.hpp"

#include <cassert>
#include <filesystem>
#include <iostream>

static int g_fail = 0;

#define CHECK(cond)                                                                 \
    do {                                                                            \
        if (!(cond)) {                                                              \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << " " #cond "\n"; \
            g_fail++;                                                               \
        }                                                                           \
    } while (0)

int main() {
    const std::filesystem::path root = std::filesystem::path(__FILE__).parent_path().parent_path();
    const auto exchanges = root / "data" / "exchanges.txt";

    Engine eng;
    eng.generate_from(exchanges, 1);
    CHECK(eng.table.size() > 50);
    verify_perfect(eng.banks.current(), eng.table);

    const Instrument* aapl = eng.lookup_symbol("AAPL", 1);
    CHECK(aapl != nullptr);
    CHECK(aapl->display == "AAPL");
    CHECK(aapl->venue == 1);

    const Instrument* jpm = eng.lookup_symbol("JPM", 2);
    CHECK(jpm != nullptr);
    CHECK(jpm->display == "JPM");

    const Instrument* arca = eng.lookup_symbol("1001", 3, true);
    CHECK(arca != nullptr);

    // Same ASCII ticker, different venue id → different packed keys.
    CHECK(eng.lookup_symbol("AAPL", 2) == nullptr);

    // Mapping aliases share instrument id with the canonical CQS name.
    const Instrument* brka = eng.lookup_symbol("BRK.A", 1);
    const Instrument* brka2 = eng.lookup_symbol("BRKA", 1);
    CHECK(brka != nullptr && brka2 != nullptr);
    CHECK(brka->id == brka2->id);

    // Hitless intraday switch: rebuild inactive, swap, lookups still work.
    const int before = eng.banks.active;
    eng.update_from(exchanges, 7);
    CHECK(eng.banks.active != before);
    CHECK(eng.lookup_symbol("MSFT", 1) != nullptr);
    verify_perfect(eng.banks.current(), eng.table);

    // Packed venue XOR is in the high byte.
    const uint64_t p = apply_venue(pack_alpha("IBM"), 2);
    CHECK(((p >> 56) & 0xFF) == 2);

    if (g_fail) {
        std::cerr << g_fail << " checks failed\n";
        return 1;
    }
    std::cout << "ok  symbols=" << eng.table.size()
              << " slots=" << eng.banks.current().n_slots << "\n";
    return 0;
}
