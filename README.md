# Equities language benchmarks (Java, Rust, C++, Python)

Single-threaded CPU comparison of **Java 21 (HotSpot / G1)**, **Rust (release, thin LTO)**, **C++ (g++ -O3 -flto)**, and **CPython 3.12** on workloads from an equities stack: a limit-order book, FIX NewOrderSingle decode, pre-trade risk, and market-data aggregation (in-memory ticks and packed binary).

All four languages implement the same spec (`benches/SPEC.md`), consume the same seeded workload, and emit a checksum. If checksums diverge, the timings are invalid.

The language trees (`java/`, `rust/`, `cpp/`, `python/`) are standalone modules. They run as **Linux containers** so macOS / Windows / Linux hosts compare the same OS.

## Benches

| Name | What it measures |
|---|---|
| `order-book` | Price-time matching: add / cancel / market, FIFO levels, maker-price fills |
| `fix-parse` | FIX 4.4 `35=D` tag scan into numeric fields |
| `risk` | Per-account qty, position, notional, 2% price collar |
| `market-data` | VWAP + 100-tick OHLC bars |
| `binary-md` | Little-endian 32-byte ticks into the same aggregator |

## Run (Docker or Podman)

```bash
./run-compose.sh          # 1,000,000 ops, all four languages
./run-compose.sh --quick  # 100,000 ops
./run-compose.sh --full   # 2,000,000 ops
./run-compose.sh --cpp-only --python-only
```

The script works on **macOS**, **Linux**, and **Windows Git Bash**. It uses `docker compose` if a Docker engine is up, otherwise `podman compose` / `podman-compose`.

Benches run **one after another** so a 6 GiB Podman VM can hold the JVM.

Output:

- `results/{java,rust,cpp,python}.json`, `results/meta.json`
- `reports/latest/index.html` (charts, environment, rerun instructions)
- `reports/latest/REPORT.md`

Open `reports/latest/index.html` in a browser. A checked-in sample is in [`reports/sample/`](reports/sample/REPORT.md) (may predate C++/Python; rerun compose for a four-language report).

## Standalone

```bash
cd java && ./gradlew run --args='--ops 100000 --output ../results/java.json'
cd rust && cargo run --release -- --ops 100000 --output ../results/rust.json
cd cpp && cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build \
  && ./build/cpp-bench --ops 100000 --output ../results/cpp.json
cd python && PYTHONPATH=src python -m equities_bench --ops 100000 --output ../results/python.json
```

Host runs are **not** the published comparison. Use compose for numbers you intend to quote.

## Fairness notes

- Same algorithms and data layout (array-backed book, dense order ids, preallocated orders).
- Timed path does not allocate after setup (Python still allocates at the interpreter level).
- Per-op timers are included in every language.
- Java: `-Xms1g -Xmx1g -XX:+AlwaysPreTouch -XX:+UseG1GC`.
- Rust: `--release`, `lto = "thin"`, `codegen-units = 1`.
- C++: `-O3 -flto`, C++20.
- Python: CPython 3.12, no extra JIT. Expect it to trail the compiled languages.
- Results are for **one Linux VM / container host**. Apple Silicon reports `aarch64`; GitHub Actions reports `x86_64`. Do not mix those numbers.

## Layout

```
java/                 Java 21 Gradle module (package com.equities.bench)
rust/src/             Cargo library + CLI (book, fix, risk, md, harness)
cpp/src/              C++20 headers + CLI (same module names)
python/src/equities_bench/   CPython package (same module names)
high-speed-storage/   US 11,948,192 perfect-hash symbol lookup (C++)
report/               Python charts + HTML
benches/SPEC.md
docker-compose.yml
run-compose.sh
```
