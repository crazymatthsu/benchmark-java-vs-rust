# high-speed-storage

Software model of [US 11,948,192](https://patents.google.com/patent/US11948192B2/en)
(*Method and system for providing high-speed storage and retrieval of information*,
JPMorgan Chase, 2024): constant-time lookup of per-instrument fields (last trade,
halt, easy-to-borrow) for symbols that look different on each venue.

This is an independent C++ model of the **published** algorithm (perfect hash +
dual mapping banks). It is not FPGA bitstream, not a product, and not legal advice.

## What the patent is solving

Ultra-low-latency trading needs to map an incoming ticker (NASDAQ `AAPL`, CQS
`BRK.A`, ARCA numeric id, …) onto one instrument record **without** a database
index and **without** stopping the book for an intraday symbol-file change.

The disclosed design:

1. **Offline generate** a deterministic **perfect hash** over the known symbol set.
2. **Program** hash coefficients (`a1..a4`) and mapping memories (`G_Map{0:2}`)
   as if they were FPGA registers / DMA tables.
3. **Online lookup** hashes a 64-bit packed symbol in a few multiplies + XORs
   and uses the result as a dense instrument index.
4. **Intraday update** writes a second (**inactive**) bank, then **double-buffers**
   to the active bank between messages (hitless).

## Mapping to this code

| Patent | Code |
|---|---|
| Symbol file list (`exchanges.txt`) | `data/exchanges.txt` |
| Listing / mapping symbol files | `data/*.txt` |
| Venue id XOR into unused symbol byte | `symbol.hpp` `pack_symbol` |
| Vector hash: 4×16-bit DSP multiply, XOR | `hash.hpp` `vector_hash` |
| N hashes `H0..H2`, `G_Map{0:2}` | `mphf.hpp` three mapping tables |
| Host register / DMA / hash-state files | `files.hpp` generate mode |
| Active (RO) + inactive (RW) banks | `banks.hpp` |
| Hitless bank switch | `Engine::activate_inactive` |
| SymbolTable (not duplicated) | `InstrumentTable` |

Lookup is O(1): `slot = (H1(key) + G[H0(key)]) mod M`. Generate fills the `g[]`
displacement table (CHD-style, the software stand-in for FPGA `G_Map`) so every
symbol lands in a unique slot.

## Build

```bash
cd high-speed-storage
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/hss generate --exchanges data/exchanges.txt --out build/state
./build/hss lookup --state build/state --symbol AAPL --venue 1
./build/hss bench --state build/state --ops 1000000
./build/hss-test
```

`generate` rebuilds the **inactive** bank and switches it in. `update` is the
same path (intraday file change).
