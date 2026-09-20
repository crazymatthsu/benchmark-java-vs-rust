# Java 21 vs Rust — equities trading benchmarks

Single-threaded CPU comparison of **Java 21 (HotSpot / G1)** and **Rust (release, thin LTO)** on workloads that show up in an equities stack: a limit-order book, FIX NewOrderSingle decode, pre-trade risk, and market-data aggregation (in-memory ticks and packed binary).

Both languages implement the same spec (`benches/SPEC.md`), consume the same seeded workload, and emit a checksum. If checksums diverge, the timings are invalid.

The two language trees (`java/`, `rust/`) are standalone modules in this repo (Gradle and Cargo). They run as **Linux containers** so macOS / Windows / Linux hosts compare the same OS.

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
./run-compose.sh          # 1,000,000 ops
./run-compose.sh --quick  # 100,000 ops
./run-compose.sh --full   # 2,000,000 ops
```

The script works on **macOS**, **Linux**, and **Windows Git Bash**. It uses `docker compose` if a Docker engine is up, otherwise `podman compose` / `podman-compose`.

Images are Linux (`eclipse-temurin:21` and `rust:bookworm`). Benches run **one after another** so a 6 GiB Podman VM can hold the JVM.

Output:

- `results/java.json`, `results/rust.json`, `results/meta.json`
- `reports/latest/index.html` (charts, environment, rerun instructions)
- `reports/latest/REPORT.md`

Open `reports/latest/index.html` in a browser. A checked-in sample from this Mac’s Podman Linux VM (aarch64, 1M ops, checksums matched) is in [`reports/sample/`](reports/sample/REPORT.md).

## Standalone

Java 21 + Gradle wrapper:

```bash
cd java
./gradlew run --args='--ops 100000 --output ../results/java.json'
```

Rust:

```bash
cd rust
cargo run --release -- --ops 100000 --output ../results/rust.json
```

Those host runs are **not** the published comparison (macOS vs Linux, different glibc, etc.). Use compose for numbers you intend to quote.

## Fairness notes

- Same algorithms and data layout (array-backed book, dense order ids, preallocated orders).
- Timed path does not allocate after setup.
- Per-op timers are included in both languages.
- Java flags: `-Xms1g -Xmx1g -XX:+AlwaysPreTouch -XX:+UseG1GC`.
- Rust: `--release`, `lto = "thin"`, `codegen-units = 1`.
- Results are for **one Linux VM / container host**. Apple Silicon reports `aarch64`; GitHub Actions reports `x86_64`. Do not mix those numbers.

## Layout

```
java/                 Java 21 Gradle module (package com.equities.bench)
rust/src/
  lib.rs              crate root
  main.rs             CLI
  constants.rs rng.rs workload.rs env.rs
  book/               matching engine
  fix/                FIX 4.4 codec
  risk/               pre-trade risk
  md/                 tick aggregation + binary records
  harness/            CLI, timers, JSON
report/               Python charts + HTML
benches/SPEC.md
docker-compose.yml
run-compose.sh
```
