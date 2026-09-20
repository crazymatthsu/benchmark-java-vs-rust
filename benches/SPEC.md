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

**Fill checksum** (wrapping u64), per fill:

```
checksum = checksum * MIX + takerId
checksum = checksum * MIX + makerId
checksum = checksum * MIX + price
checksum = checksum * MIX + fillQty
```

After all ops, for `symbol = 0 .. nSymbols-1`:

```
checksum = checksum * MIX + bidTotalQty
checksum = checksum * MIX + askTotalQty
checksum = checksum * MIX + fillCount
checksum = checksum * MIX + cancelHits
```

Bid/ask totals are sums of `levels[p].totalQty` over occupied sides (`p` from `PRICE_MIN` to `PRICE_MAX`; bid vs ask is determined by walking levels — actually sum by scanning all resting orders with `qty > 0` grouped by side, or equivalently sum `totalQty` on prices where we also know side... **A level is not side-tagged.** Bids and asks at the same price would collide.

**Rule:** a given price on a given symbol is only used by one side in practice if the book never locks. To keep totals unambiguous, store two level arrays: `bids[price]` and `asks[price]`.

### Bid/ask arrays

- `bids[price]` and `asks[price]` each have `ids[]`, `size`, `totalQty`.
- Rest buys into `bids[price]`, sells into `asks[price]`.
- `bidTotalQty` = sum of `bids[p].totalQty`; `askTotalQty` = sum of `asks[p].totalQty`.

## FIX parse

Each bench uses a **fresh** `XorShift64(seed)` (independent of the order-book generator).

For `i in 0 .. nOps-1` generate one NewOrderSingle (`35=D`) with SOH = `0x01`:

```
body =
  35=D SOH
  34=<i+1> SOH
  49=SENDER SOH
  56=TARGET SOH
  11=<clOrdId> SOH
  55=SYM<pad2(symbol)> SOH
  54=<1 if BUY else 2> SOH
  38=<qty> SOH
  44=<price> SOH
  40=2 SOH
  59=0 SOH
```

Fields from RNG (same order): `clOrdId = i+1`, `symbol = nextBounded(nSymbols)`, `side = nextBounded(2)`, `qty = 1 + nextBounded(100)`, `price = PRICE_MID + nextBounded(PRICE_SPAN) - PRICE_SPAN/2`.

`9=` body length is the ASCII length of `body` with **no padding**. Header is `8=FIX.4.4 SOH 9=<len> SOH`. Trailer `10=<pad3(sum(bytes of header+body) % 256)> SOH`.

**Timed path:** scan tags in the stored bytes. Extract `11, 55, 54, 38, 44`. Symbol value is the integer after the prefix `SYM`. Mix:

```
checksum = checksum * MIX + clOrdId
checksum = checksum * MIX + symbol
checksum = checksum * MIX + side
checksum = checksum * MIX + qty
checksum = checksum * MIX + price
```

## Risk

Reuse the **order-book workload** arrays. Skip `OP_CANCEL`.

Per-account `position[nSymbols]` (signed), per-symbol `lastPx` init `PRICE_MID`.

Limits:

```
MAX_ORDER_QTY = 10000
MAX_POSITION  = 50000
MAX_NOTIONAL  = 1_000_000_000_000   // 10^12
COLLAR_BPS    = 200                 // 2%
```

For ADD/MARKET:

- `px = price == 0 ? lastPx[symbol] : price`
- `signedQty = side == BUY ? qty : -qty`
- `notional = px * qty` (unsigned-style positive long)
- Reject (no state change) if any of:
  1. `qty <= 0` or `qty > MAX_ORDER_QTY`
  2. `abs(position[acct][sym] + signedQty) > MAX_POSITION`
  3. `acctNotional[acct] + notional > MAX_NOTIONAL`
  4. `lastPx[sym] > 0` and `abs(px - lastPx[sym]) * 10000 > lastPx[sym] * COLLAR_BPS`
- Else accept: apply position, add notional, `lastPx[sym] = px`.

Checksum after all ops:

```
checksum = MIX*checksum + accepts
checksum = MIX*checksum + rejects
for acct in 0..nAccounts-1:
  for sym in 0..nSymbols-1:
    checksum = MIX*checksum + position[acct][sym]   // two's complement bits
```

## Market data

Fresh `XorShift64(seed)`. For each tick: `symbol`, `price`, `qty` with the same formulas as ADD (three RNG calls).

Per symbol aggregator:

- `open, high, low, close, notional, volume, ticks, bars`
- `low` init `i64::MAX` / `Long.MAX_VALUE`
- On first tick (`ticks == 0` before increment): `open = price`
- `high = max(high, price)` (high init `i64::MIN`)
- `low = min(low, price)`
- `close = price`
- `notional += price * qty`, `volume += qty`, `ticks += 1`
- If `ticks % 100 == 0`: `bars += 1`

Checksum per symbol `0 .. nSymbols-1`:

```
mix open, high, low, close, notional, volume, bars
```

## Binary market data

Fresh `XorShift64(seed)`. Each record is 32 bytes little-endian:

```
u32 symbol | u32 pad=0 | i64 price | i64 qty | u64 ts
```

Field generation: `symbol`, `price`, `qty` as market-data ticks, `ts = i`.

Timed path: decode each record with manual little-endian reads (no extra libraries) and apply the **same** aggregator as `market-data`. Checksum identical in structure (so `market-data` and `binary-md` checksums must match for the same seed/ops/symbols).

## Harness

For each bench:

1. **Setup** (not timed): allocate, generate inputs, precreate order objects / vectors.
2. **Warmup** (not timed): apply the first `warmup` operations on a throwaway instance (`warmup = min(warmup, ops)`).
3. **Measure:** new instance, for each op record `ns = t1 - t0`, then sort samples.
4. Percentile index: `floor(p * (n - 1))` for `p in {0.50, 0.90, 0.99, 0.999}`.
5. `throughput_ops_s = ops / (elapsed_ns / 1e9)`.
6. Write one JSON file per language.

## JSON (per language)

Top-level keys: `language`, `runtime`, `os`, `arch`, `cpus`, `mem_bytes`, `os_pretty`, `cpu_model`, `container_runtime`, `seed`, `ops`, `warmup`, `symbols`, `accounts`, `benchmarks[]`.

Each benchmark: `name`, `ops`, `elapsed_ns`, `throughput_ops_s`, `checksum` (unsigned decimal string), `latency_ns` `{min,p50,p90,p99,p999,max}`, `stats` (string-keyed numbers).
