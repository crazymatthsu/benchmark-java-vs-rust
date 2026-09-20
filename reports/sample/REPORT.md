# Java 21 vs Rust — equities benchmark report

Generated **2026-09-20 12:34:17** UTC.

## Environment

| | Java | Rust |
|---|---|---|
| Runtime | OpenJDK 64-Bit Server VM 21.0.12 (21.0.12+8-LTS) | rustc 1.98.1 (48a229cea 2026-09-01) |
| OS | Ubuntu 22.04.5 LTS | Debian GNU/Linux 12 (bookworm) |
| Arch | aarch64 | aarch64 |
| CPUs | 7 | 7 |
| Container | podman | podman |
| CPU | implementer 0x61 part 0x000 | implementer 0x61 part 0x000 |
| Ops / warmup | 1000000 / 50000 | same |
| Seed | 869193496018642825 | 869193496018642825 |

Host (from runner): `podman podman version 5.8.2` on `Darwin/arm64`.

Checksums **MATCH**.

## Throughput and latency

![Throughput](throughput.png)

![p50 latency](latency_p50.png)

![p99 latency](latency_p99.png)

| Bench | Java ops/s | Rust ops/s | Rust/Java | Java p50 ns | Rust p50 ns | Java p99 ns | Rust p99 ns | Checksum |
|---|---:|---:|---:|---:|---:|---:|---:|---|
| `order-book` | 4.75 M | 7.62 M | 1.61x | 125 | 83 | 1000 | 583 | yes |
| `fix-parse` | 8.67 M | 10.71 M | 1.24x | 83 | 83 | 250 | 84 | yes |
| `risk` | 27.18 M | 26.65 M | 0.98x | 0 | 0 | 42 | 42 | yes |
| `market-data` | 30.85 M | 28.43 M | 0.92x | 0 | 0 | 42 | 42 | yes |
| `binary-md` | 28.87 M | 29.21 M | 1.01x | 0 | 0 | 42 | 42 | yes |

Geometric-mean throughput speedup (Rust / Java): **1.13x**.

## How this run was produced

1. Linux containers (Temurin 21 JRE and Debian slim + release Rust).
2. Same seed, op count, symbols, accounts, and algorithms (`benches/SPEC.md`).
3. Warmup on a throwaway instance, then one measured pass with per-op timers.
4. Single-threaded. This is a language/runtime comparison, not a scaling study.
5. Command: `./run-compose.sh`

## How to rerun

From the repo root, with Docker or Podman:

```bash
./run-compose.sh          # 1,000,000 ops (default)
./run-compose.sh --quick  # 100,000 ops smoke run
./run-compose.sh --full   # 2,000,000 ops
```

The script works on macOS, Linux, and Windows Git Bash. It detects `docker compose`,
`docker-compose`, `podman compose`, or `podman-compose`. Reports land in
`reports/latest/` (HTML, Markdown, PNG) and `results/*.json`.

Standalone (no compose), after installing JDK 21 / Rust:

```bash
cd java && ./gradlew run --args='--ops 100000 --output ../results/java.json'
cd rust && cargo run --release -- --ops 100000 --output ../results/rust.json
```

## Methodology excerpt

```
# Equities benchmark specification

Java 21 and Rust implement this spec line-for-line. Timed loops, PRNG, matching
rules, and checksum mixing must match so a checksum mismatch is a bug, not a
language difference.

## Goals

Compare **single-threaded** CPU work typical of an equities trading stack:

| Bench | What it models |
|---|---|
| `order-book` | Price-time matching engine (add / cancel / market) |
| `fix-parse` | FIX 4.4 NewOrderSingle decode (gateway) |
| `risk` | Pre-trade limits: qty, position, notional, price collar |
| `market-data` | Tick aggregation: VWAP + 100-tick OHLC bars |
| `binary-md` | Packed 32-byte tick decode + the same aggregator |

I/O, networking, persistence, and multi-thread scaling are out of scope.

## Fairness rules

- Same seed, op counts, symbols, and accounts.
- Same algorithms and data layout (array-backed book, preallocated orders).
- No allocations on the timed path after setup.
- Per-op `nanoTime` / `Instant::now` around each operation; both languages pay it.
- Checksums printed as **unsigned decimal strings** (JSON numbers lose u64 precision).
- Java: HotSpot 21, G1, fixed heap, `AlwaysPreTouch`.
- Rust: `--release`, thin LTO, `codegen-units = 1`. Default hasher is unused; IDs are dense arrays.

## Constants

```
PRICE_MIN  = 1
PRICE_MAX  = 20000
PRICE_MID  = 10000
PRICE_SPAN = 200          // generated price = MID + bounded(SPAN) - SPAN/2
MIX        = 1000003      // wrapping u64 checksum mixer
DEFAULT_SEED = 0x0C0FFEE123456789
BUY = 0, SELL = 1
TIF_GTC = 0, TIF_IOC = 1
OP_ADD = 0, OP_CANCEL = 1, OP_MARKET = 2
```

Default CLI: `--ops 1000000 --warmup 50000 --symbols 32 --accounts 256`.

## PRNG

XorShift64 (Marsaglia), 64-bit state, **logical** right shift:

```
x ^= x << 13
x ^= x >> 7     // unsigned
x ^= x << 17
```

If seed is `0`, use `DEFAULT_SEED`. `nextBounded(n)` = unsigned `nextU64() % n` with `n > 0`.

## Workload generation (order-book and risk)

One `XorShift64(seed)`. `nextId` starts at `1`. For each `i in 0..nOps`:

1. `r = nextBounded(100)`
2. If `r < 70` **or** `nextId == 1` → **ADD**
   - `orderId = nextId++`
   - `symbol = nextBounded(nSymbols)`
   - `side = nextBounded(2)`
   - `price = PRICE_MID + nextBounded(PRICE_SPAN) - PRICE_SPAN/2`
   - `qty = 1 + nextBounded(100)`
   - `tif = nextBounded(10) == 0 ? IOC : GTC`
   - `account = nextBounded(nAccounts)`
3. Else if `r < 90` → **CANCEL**
   - `orderId = 1 + nextBounded(nextId - 1)`
   - `account = nextBounded(nAccounts)`
   - other fields 0
4. Else → **MARKET**
   - `orderId = nextId++`
   - `symbol = nextBounded(nSymbols)`
   - `side = nextBounded(2)`
   - `price = 0`
   - `qty = 1 + nextBounded(100)`
   - `tif = IOC`
   - `account = nextBounded(nAccounts)`

RNG call order inside each branch is exactly as listed.

## Order book

One book per symbol. Prices are integers in `[PRICE_MIN, PRICE_MAX]`.

**Layout**

- `orders[id]`: `{symbol, side, price, qty}` — qty `0` means inactive (fill, cancel, or never rested).
- Per book: `levels[price]` = FIFO list of order ids + `totalQty`.
- `bestBid` init `0`, `bestAsk` init `PRICE_MAX + 1`.

**Match** (taker vs opposite side, maker price, price-time / FIFO):

- Buy: while `qty > 0` and `bestAsk <= PRICE_MAX` and `bestAsk <= limitPrice`, hit that ask level.
- Sell: while `qty > 0` and `bestBid >= PRICE_MIN` and `bestBid >= limitPrice`, hit that bid level.
- At a level, scan ids in insertion order; skip `qty == 0`; `fill = min(takerQty, makerQty)`.
- After a level’s `totalQty` hits 0, walk `bestAsk++` / `bestBid--` until a non-empty level or bound.

**Add limit:** match first with `limitPrice = price`. If remainder `> 0` and GTC, rest on the book and update best bid/ask. IOC remainder is discarded (`iocKilled += remainder`).

**Market:** buy limit `PRICE_MAX`, sell limit `PRICE_MIN`, never rest (IOC).

**Cancel:** if `orders[id].qty > 0`, subtract from that level’s `totalQty`, set qty 0, `cancelHits++`, and advance best if that level emptied. Else `cancelMisses++`.

**Fill che

_(truncated; see benches/SPEC.md)_
```

A p50 of 0 ns means the operation was faster than the container clock (often ~40 ns).
Use throughput for those benches.

These numbers are for this hardware, this container runtime, and this workload.
They are not a universal ranking of the two languages.
