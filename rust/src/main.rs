mod binary_md;
mod c;
mod env_info;
mod fix;
mod json_out;
mod market_data;
mod order_book;
mod percentiles;
mod rng;
mod risk;
mod tick_agg;
mod workload;

use std::env;
use std::path::PathBuf;
use std::time::Instant;

use binary_md::BinaryMdBench;
use env_info::EnvInfo;
use fix::FixBench;
use json_out::{write, BenchResult};
use market_data::MarketDataBench;
use order_book::OrderBookEngine;
use risk::RiskEngine;
use workload::Workload;

fn main() {
    let cfg = Config::parse(env::args().skip(1).collect());
    if cfg.help {
        Config::print_help();
        return;
    }
    println!(
        "rust-bench ops={} warmup={} seed={} symbols={} accounts={} benches={}",
        cfg.ops, cfg.warmup, cfg.seed, cfg.symbols, cfg.accounts, cfg.benches
    );
    let env_info = EnvInfo::collect();
    let mut results = Vec::new();
    if cfg.want("order-book") {
        results.push(run_order_book(&cfg));
    }
    if cfg.want("fix-parse") {
        results.push(run_fix(&cfg));
    }
    if cfg.want("risk") {
        results.push(run_risk(&cfg));
    }
    if cfg.want("market-data") {
        results.push(run_market_data(&cfg));
    }
    if cfg.want("binary-md") {
        results.push(run_binary_md(&cfg));
    }
    write(
        &cfg.output,
        &env_info,
        &cfg.runtime_name,
        cfg.seed,
        cfg.ops as u64,
        cfg.warmup as u64,
        cfg.symbols,
        cfg.accounts,
        &results,
    )
    .expect("write json");
    println!("wrote {}", cfg.output.display());
}

fn run_order_book(cfg: &Config) -> BenchResult {
    log("generating order-book workload");
    let w = Workload::generate(cfg.seed, cfg.ops, cfg.symbols, cfg.accounts);
    let warmup = cfg.warmup.min(cfg.ops);
    log(&format!("warming order-book ({warmup} ops)"));
    {
        let mut throwaway = OrderBookEngine::new(cfg.symbols, cfg.ops as i32);
        for i in 0..warmup {
            throwaway.apply(&w, i);
        }
    }
    log("measuring order-book");
    let mut eng = OrderBookEngine::new(cfg.symbols, cfg.ops as i32);
    let mut samples = vec![0u64; cfg.ops];
    let wall = Instant::now();
    for i in 0..cfg.ops {
        let s = Instant::now();
        eng.apply(&w, i);
        samples[i] = s.elapsed().as_nanos() as u64;
    }
    let elapsed = wall.elapsed().as_nanos() as u64;
    eng.finish();
    percentiles::sort_in_place(&mut samples);
    let r = BenchResult::new("order-book", cfg.ops as u64, elapsed, eng.checksum, &samples)
        .stat("fills", eng.fill_count as i64)
        .stat("fill_qty", eng.fill_qty)
        .stat("cancel_hits", eng.cancel_hits as i64)
        .stat("cancel_misses", eng.cancel_misses as i64)
        .stat("ioc_killed", eng.ioc_killed)
        .stat("resting_adds", eng.rest_count as i64);
    print_result(&r);
    r
}

fn run_fix(cfg: &Config) -> BenchResult {
    log("generating FIX messages");
    let mut bench = FixBench::new(cfg.seed, cfg.ops, cfg.symbols);
    let warmup = cfg.warmup.min(cfg.ops);
    log("warming fix-parse");
    for i in 0..warmup {
        bench.parse(i);
    }
    bench.checksum = 0;
    log("measuring fix-parse");
    let mut samples = vec![0u64; cfg.ops];
    let wall = Instant::now();
    for i in 0..cfg.ops {
        let s = Instant::now();
        bench.parse(i);
        samples[i] = s.elapsed().as_nanos() as u64;
    }
    let elapsed = wall.elapsed().as_nanos() as u64;
    percentiles::sort_in_place(&mut samples);
    let bytes = bench.arena.len() as i64;
    let r = BenchResult::new("fix-parse", cfg.ops as u64, elapsed, bench.checksum, &samples)
        .stat("messages", cfg.ops as i64)
        .stat("bytes", bytes);
    print_result(&r);
    r
}

fn run_risk(cfg: &Config) -> BenchResult {
    log("generating risk workload");
    let w = Workload::generate(cfg.seed, cfg.ops, cfg.symbols, cfg.accounts);
    let warmup = cfg.warmup.min(cfg.ops);
    log("warming risk");
    {
        let mut throwaway = RiskEngine::new(cfg.accounts, cfg.symbols);
        for i in 0..warmup {
            throwaway.apply(&w, i);
        }
    }
    log("measuring risk");
    let mut eng = RiskEngine::new(cfg.accounts, cfg.symbols);
    let mut samples = vec![0u64; cfg.ops];
    let wall = Instant::now();
    for i in 0..cfg.ops {
        let s = Instant::now();
        eng.apply(&w, i);
        samples[i] = s.elapsed().as_nanos() as u64;
    }
    let elapsed = wall.elapsed().as_nanos() as u64;
    eng.finish();
    percentiles::sort_in_place(&mut samples);
    let r = BenchResult::new("risk", cfg.ops as u64, elapsed, eng.checksum, &samples)
        .stat("accepts", eng.accepts as i64)
        .stat("rejects", eng.rejects as i64);
    print_result(&r);
    r
}

fn run_market_data(cfg: &Config) -> BenchResult {
    log("generating market-data ticks");
    let mut bench = MarketDataBench::new(cfg.seed, cfg.ops, cfg.symbols);
    let warmup = cfg.warmup.min(cfg.ops);
    log("warming market-data");
    for i in 0..warmup {
        bench.apply(i);
    }
    bench.reset_aggs();
    log("measuring market-data");
    let mut samples = vec![0u64; cfg.ops];
    let wall = Instant::now();
    for i in 0..cfg.ops {
        let s = Instant::now();
        bench.apply(i);
        samples[i] = s.elapsed().as_nanos() as u64;
    }
    let elapsed = wall.elapsed().as_nanos() as u64;
    bench.finish();
    percentiles::sort_in_place(&mut samples);
    let r = BenchResult::new(
        "market-data",
        cfg.ops as u64,
        elapsed,
        bench.checksum,
        &samples,
    )
    .stat("ticks", cfg.ops as i64);
    print_result(&r);
    r
}

fn run_binary_md(cfg: &Config) -> BenchResult {
    log("generating binary ticks");
    let mut bench = BinaryMdBench::new(cfg.seed, cfg.ops, cfg.symbols);
    let warmup = cfg.warmup.min(cfg.ops);
    log("warming binary-md");
    for i in 0..warmup {
        bench.apply(i);
    }
    bench.reset_aggs();
    log("measuring binary-md");
    let mut samples = vec![0u64; cfg.ops];
    let wall = Instant::now();
    for i in 0..cfg.ops {
        let s = Instant::now();
        bench.apply(i);
        samples[i] = s.elapsed().as_nanos() as u64;
    }
    let elapsed = wall.elapsed().as_nanos() as u64;
    bench.finish();
    percentiles::sort_in_place(&mut samples);
    let r = BenchResult::new("binary-md", cfg.ops as u64, elapsed, bench.checksum, &samples)
        .stat("ticks", cfg.ops as i64)
        .stat("bytes", (cfg.ops * binary_md::REC) as i64);
    print_result(&r);
    r
}

fn print_result(r: &BenchResult) {
    println!(
        "=== {} ===  {:.0} ops/s  p50={} ns  p99={} ns  checksum={}",
        r.name, r.throughput_ops_s, r.p50_ns, r.p99_ns, r.checksum
    );
}

fn log(msg: &str) {
    println!("{msg}");
}

struct Config {
    ops: usize,
    warmup: usize,
    seed: u64,
    symbols: i32,
    accounts: i32,
    output: PathBuf,
    runtime_name: String,
    benches: String,
    help: bool,
}

impl Config {
    fn want(&self, name: &str) -> bool {
        if self.benches == "all" {
            return true;
        }
        self.benches.split(',').any(|p| p.trim() == name)
    }

    fn parse(args: Vec<String>) -> Self {
        let mut c = Self {
            ops: 1_000_000,
            warmup: 50_000,
            seed: c::DEFAULT_SEED,
            symbols: 32,
            accounts: 256,
            output: PathBuf::from("results/rust.json"),
            runtime_name: "unknown".into(),
            benches: "all".into(),
            help: false,
        };
        let mut i = 0;
        while i < args.len() {
            match args[i].as_str() {
                "-h" | "--help" => c.help = true,
                "--ops" => {
                    i += 1;
                    c.ops = args[i].parse().expect("ops");
                }
                "--warmup" => {
                    i += 1;
                    c.warmup = args[i].parse().expect("warmup");
                }
                "--seed" => {
                    i += 1;
                    c.seed = parse_seed(&args[i]);
                }
                "--symbols" => {
                    i += 1;
                    c.symbols = args[i].parse().expect("symbols");
                }
                "--accounts" => {
                    i += 1;
                    c.accounts = args[i].parse().expect("accounts");
                }
                "--output" => {
                    i += 1;
                    c.output = PathBuf::from(&args[i]);
                }
                "--runtime-name" => {
                    i += 1;
                    c.runtime_name = args[i].clone();
                }
                "--bench" => {
                    i += 1;
                    c.benches = args[i].clone();
                }
                other => panic!("unknown arg: {other}"),
            }
            i += 1;
        }
        assert!(c.ops > 0 && c.symbols > 0 && c.accounts > 0);
        c
    }

    fn print_help() {
        println!(
            "rust-bench — equities Rust harness
  --ops N              operations per bench (default 1000000)
  --warmup N           warmup ops (default 50000)
  --seed HEX_OR_DEC    default 0x0C0FFEE123456789
  --symbols N          default 32
  --accounts N         default 256
  --bench all|name,... order-book,fix-parse,risk,market-data,binary-md
  --output PATH
  --runtime-name NAME  docker|podman|host"
        );
    }
}

fn parse_seed(s: &str) -> u64 {
    if let Some(hex) = s.strip_prefix("0x").or_else(|| s.strip_prefix("0X")) {
        u64::from_str_radix(hex, 16).expect("hex seed")
    } else {
        s.parse::<u64>().expect("seed")
    }
}
