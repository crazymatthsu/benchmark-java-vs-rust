from __future__ import annotations

import sys
import time
from dataclasses import dataclass
from pathlib import Path

from .. import constants as c
from ..book import OrderBookEngine
from ..env import collect
from ..fix import FixBench
from ..md import REC, BinaryMdBench, MarketDataBench
from ..risk import RiskEngine
from ..workload import Workload
from . import percentiles
from .json_out import BenchResult, write


@dataclass
class Config:
    ops: int = 1_000_000
    warmup: int = 50_000
    seed: int = c.DEFAULT_SEED
    symbols: int = 32
    accounts: int = 256
    output: Path = Path("results/python.json")
    runtime_name: str = "unknown"
    benches: str = "all"
    help: bool = False

    def want(self, name: str) -> bool:
        if self.benches == "all":
            return True
        return name in [p.strip() for p in self.benches.split(",")]


def parse_seed(s: str) -> int:
    if s.startswith("0x") or s.startswith("0X"):
        return int(s[2:], 16)
    return int(s)


def parse_args(argv: list[str]) -> Config:
    cfg = Config()
    i = 0
    while i < len(argv):
        a = argv[i]
        if a in ("-h", "--help"):
            cfg.help = True
        elif a == "--ops":
            i += 1
            cfg.ops = int(argv[i])
        elif a == "--warmup":
            i += 1
            cfg.warmup = int(argv[i])
        elif a == "--seed":
            i += 1
            cfg.seed = parse_seed(argv[i])
        elif a == "--symbols":
            i += 1
            cfg.symbols = int(argv[i])
        elif a == "--accounts":
            i += 1
            cfg.accounts = int(argv[i])
        elif a == "--output":
            i += 1
            cfg.output = Path(argv[i])
        elif a == "--runtime-name":
            i += 1
            cfg.runtime_name = argv[i]
        elif a == "--bench":
            i += 1
            cfg.benches = argv[i]
        else:
            raise SystemExit(f"unknown arg: {a}")
        i += 1
    if cfg.ops <= 0 or cfg.symbols <= 0 or cfg.accounts <= 0:
        raise SystemExit("ops/symbols/accounts must be > 0")
    return cfg


def print_help() -> None:
    print(
        """python-bench — equities CPython harness
  --ops N              operations per bench (default 1000000)
  --warmup N           warmup ops (default 50000)
  --seed HEX_OR_DEC    default 0x0C0FFEE123456789
  --symbols N          default 32
  --accounts N         default 256
  --bench all|name,... order-book,fix-parse,risk,market-data,binary-md
  --output PATH
  --runtime-name NAME  docker|podman|host"""
    )


def print_result(r: BenchResult) -> None:
    print(
        f"=== {r.name} ===  {r.throughput_ops_s:.0f} ops/s  p50={r.p50_ns} ns  "
        f"p99={r.p99_ns} ns  checksum={r.checksum}"
    )


def run_order_book(cfg: Config) -> BenchResult:
    print("generating order-book workload")
    w = Workload.generate(cfg.seed, cfg.ops, cfg.symbols, cfg.accounts)
    warmup = min(cfg.warmup, cfg.ops)
    print(f"warming order-book ({warmup} ops)")
    throwaway = OrderBookEngine(cfg.symbols, cfg.ops)
    for i in range(warmup):
        throwaway.apply(w, i)
    del throwaway
    print("measuring order-book")
    eng = OrderBookEngine(cfg.symbols, cfg.ops)
    samples = [0] * cfg.ops
    t0 = time.perf_counter_ns()
    for i in range(cfg.ops):
        s = time.perf_counter_ns()
        eng.apply(w, i)
        samples[i] = time.perf_counter_ns() - s
    elapsed = time.perf_counter_ns() - t0
    eng.finish()
    percentiles.sort_in_place(samples)
    r = (
        BenchResult("order-book", cfg.ops, elapsed, eng.checksum, samples)
        .stat("fills", eng.fill_count)
        .stat("fill_qty", eng.fill_qty)
        .stat("cancel_hits", eng.cancel_hits)
        .stat("cancel_misses", eng.cancel_misses)
        .stat("ioc_killed", eng.ioc_killed)
        .stat("resting_adds", eng.rest_count)
    )
    print_result(r)
    return r


def run_fix(cfg: Config) -> BenchResult:
    print("generating FIX messages")
    bench = FixBench(cfg.seed, cfg.ops, cfg.symbols)
    warmup = min(cfg.warmup, cfg.ops)
    print("warming fix-parse")
    for i in range(warmup):
        bench.parse(i)
    bench.checksum = 0
    print("measuring fix-parse")
    samples = [0] * cfg.ops
    t0 = time.perf_counter_ns()
    for i in range(cfg.ops):
        s = time.perf_counter_ns()
        bench.parse(i)
        samples[i] = time.perf_counter_ns() - s
    elapsed = time.perf_counter_ns() - t0
    percentiles.sort_in_place(samples)
    r = (
        BenchResult("fix-parse", cfg.ops, elapsed, bench.checksum, samples)
        .stat("messages", cfg.ops)
        .stat("bytes", len(bench.arena))
    )
    print_result(r)
    return r


def run_risk(cfg: Config) -> BenchResult:
    print("generating risk workload")
    w = Workload.generate(cfg.seed, cfg.ops, cfg.symbols, cfg.accounts)
    warmup = min(cfg.warmup, cfg.ops)
    print("warming risk")
    throwaway = RiskEngine(cfg.accounts, cfg.symbols)
    for i in range(warmup):
        throwaway.apply(w, i)
    del throwaway
    print("measuring risk")
    eng = RiskEngine(cfg.accounts, cfg.symbols)
    samples = [0] * cfg.ops
    t0 = time.perf_counter_ns()
    for i in range(cfg.ops):
        s = time.perf_counter_ns()
        eng.apply(w, i)
        samples[i] = time.perf_counter_ns() - s
    elapsed = time.perf_counter_ns() - t0
    eng.finish()
    percentiles.sort_in_place(samples)
    r = BenchResult("risk", cfg.ops, elapsed, eng.checksum, samples).stat("accepts", eng.accepts).stat(
        "rejects", eng.rejects
    )
    print_result(r)
    return r


def run_market_data(cfg: Config) -> BenchResult:
    print("generating market-data ticks")
    bench = MarketDataBench(cfg.seed, cfg.ops, cfg.symbols)
    warmup = min(cfg.warmup, cfg.ops)
    print("warming market-data")
    for i in range(warmup):
        bench.apply(i)
    bench.reset_aggs()
    print("measuring market-data")
    samples = [0] * cfg.ops
    t0 = time.perf_counter_ns()
    for i in range(cfg.ops):
        s = time.perf_counter_ns()
        bench.apply(i)
        samples[i] = time.perf_counter_ns() - s
    elapsed = time.perf_counter_ns() - t0
    bench.finish()
    percentiles.sort_in_place(samples)
    r = BenchResult("market-data", cfg.ops, elapsed, bench.checksum, samples).stat("ticks", cfg.ops)
    print_result(r)
    return r


def run_binary_md(cfg: Config) -> BenchResult:
    print("generating binary ticks")
    bench = BinaryMdBench(cfg.seed, cfg.ops, cfg.symbols)
    warmup = min(cfg.warmup, cfg.ops)
    print("warming binary-md")
    for i in range(warmup):
        bench.apply(i)
    bench.reset_aggs()
    print("measuring binary-md")
    samples = [0] * cfg.ops
    t0 = time.perf_counter_ns()
    for i in range(cfg.ops):
        s = time.perf_counter_ns()
        bench.apply(i)
        samples[i] = time.perf_counter_ns() - s
    elapsed = time.perf_counter_ns() - t0
    bench.finish()
    percentiles.sort_in_place(samples)
    r = (
        BenchResult("binary-md", cfg.ops, elapsed, bench.checksum, samples)
        .stat("ticks", cfg.ops)
        .stat("bytes", cfg.ops * REC)
    )
    print_result(r)
    return r


def run(argv: list[str] | None = None) -> None:
    cfg = parse_args(sys.argv[1:] if argv is None else argv)
    if cfg.help:
        print_help()
        return
    print(
        f"python-bench ops={cfg.ops} warmup={cfg.warmup} seed={cfg.seed} "
        f"symbols={cfg.symbols} accounts={cfg.accounts} benches={cfg.benches}"
    )
    env = collect()
    results: list[BenchResult] = []
    if cfg.want("order-book"):
        results.append(run_order_book(cfg))
    if cfg.want("fix-parse"):
        results.append(run_fix(cfg))
    if cfg.want("risk"):
        results.append(run_risk(cfg))
    if cfg.want("market-data"):
        results.append(run_market_data(cfg))
    if cfg.want("binary-md"):
        results.append(run_binary_md(cfg))
    write(
        cfg.output,
        env,
        cfg.runtime_name,
        cfg.seed,
        cfg.ops,
        cfg.warmup,
        cfg.symbols,
        cfg.accounts,
        results,
    )
    print(f"wrote {cfg.output}")
