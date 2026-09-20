use std::fs;
use std::io::{self, Write};
use std::path::Path;

use crate::env_info::EnvInfo;

pub struct BenchResult {
    pub name: String,
    pub ops: u64,
    pub elapsed_ns: u64,
    pub throughput_ops_s: f64,
    pub checksum: String,
    pub min_ns: u64,
    pub p50_ns: u64,
    pub p90_ns: u64,
    pub p99_ns: u64,
    pub p999_ns: u64,
    pub max_ns: u64,
    pub stats: Vec<(String, i64)>,
}

impl BenchResult {
    pub fn new(
        name: &str,
        ops: u64,
        elapsed_ns: u64,
        checksum: u64,
        sorted: &[u64],
    ) -> Self {
        let throughput = if elapsed_ns == 0 {
            0.0
        } else {
            ops as f64 / (elapsed_ns as f64 / 1_000_000_000.0)
        };
        Self {
            name: name.to_string(),
            ops,
            elapsed_ns,
            throughput_ops_s: throughput,
            checksum: checksum.to_string(),
            min_ns: crate::percentiles::min(sorted),
            p50_ns: crate::percentiles::pct(sorted, 0.50),
            p90_ns: crate::percentiles::pct(sorted, 0.90),
            p99_ns: crate::percentiles::pct(sorted, 0.99),
            p999_ns: crate::percentiles::pct(sorted, 0.999),
            max_ns: crate::percentiles::max(sorted),
            stats: Vec::new(),
        }
    }

    pub fn stat(mut self, k: &str, v: i64) -> Self {
        self.stats.push((k.to_string(), v));
        self
    }
}

pub fn write(
    path: &Path,
    env: &EnvInfo,
    container_runtime: &str,
    seed: u64,
    ops: u64,
    warmup: u64,
    symbols: i32,
    accounts: i32,
    benches: &[BenchResult],
) -> io::Result<()> {
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent)?;
    }
    let mut sb = String::new();
    sb.push_str("{\n");
    field_i(&mut sb, "schema_version", 1, true, 2);
    field_s(&mut sb, "language", "rust", true, 2);
    field_s(&mut sb, "runtime", &env.runtime, true, 2);
    field_s(&mut sb, "os", &env.os, true, 2);
    field_s(&mut sb, "arch", &env.arch, true, 2);
    field_i(&mut sb, "cpus", i64::from(env.cpus), true, 2);
    field_u(&mut sb, "mem_bytes", env.mem_bytes, true, 2);
    field_s(&mut sb, "os_pretty", &env.os_pretty, true, 2);
    field_s(&mut sb, "cpu_model", &env.cpu_model, true, 2);
    field_s(&mut sb, "container_runtime", container_runtime, true, 2);
    field_s(&mut sb, "seed", &seed.to_string(), true, 2);
    field_u(&mut sb, "ops", ops, true, 2);
    field_u(&mut sb, "warmup", warmup, true, 2);
    field_i(&mut sb, "symbols", i64::from(symbols), true, 2);
    field_i(&mut sb, "accounts", i64::from(accounts), true, 2);
    sb.push_str("  \"benchmarks\": [\n");
    for (i, b) in benches.iter().enumerate() {
        write_bench(&mut sb, b, i + 1 < benches.len());
    }
    sb.push_str("  ]\n");
    sb.push_str("}\n");
    let mut f = fs::File::create(path)?;
    f.write_all(sb.as_bytes())?;
    Ok(())
}

fn write_bench(sb: &mut String, b: &BenchResult, comma: bool) {
    sb.push_str("    {\n");
    field_s(sb, "name", &b.name, true, 6);
    field_u(sb, "ops", b.ops, true, 6);
    field_u(sb, "elapsed_ns", b.elapsed_ns, true, 6);
    field_f(sb, "throughput_ops_s", b.throughput_ops_s, true, 6);
    field_s(sb, "checksum", &b.checksum, true, 6);
    sb.push_str("      \"latency_ns\": {\n");
    field_u(sb, "min", b.min_ns, true, 8);
    field_u(sb, "p50", b.p50_ns, true, 8);
    field_u(sb, "p90", b.p90_ns, true, 8);
    field_u(sb, "p99", b.p99_ns, true, 8);
    field_u(sb, "p999", b.p999_ns, true, 8);
    field_u(sb, "max", b.max_ns, false, 8);
    sb.push_str("      },\n");
    sb.push_str("      \"stats\": {\n");
    for (n, (k, v)) in b.stats.iter().enumerate() {
        field_i(sb, k, *v, n + 1 < b.stats.len(), 8);
    }
    sb.push_str("      }\n");
    sb.push_str("    }");
    sb.push_str(if comma { ",\n" } else { "\n" });
}

fn field_s(sb: &mut String, k: &str, v: &str, comma: bool, indent: usize) {
    sb.push_str(&" ".repeat(indent));
    sb.push('"');
    sb.push_str(k);
    sb.push_str("\": \"");
    sb.push_str(&escape(v));
    sb.push('"');
    sb.push_str(if comma { ",\n" } else { "\n" });
}

fn field_i(sb: &mut String, k: &str, v: i64, comma: bool, indent: usize) {
    sb.push_str(&" ".repeat(indent));
    sb.push('"');
    sb.push_str(k);
    sb.push_str("\": ");
    sb.push_str(&v.to_string());
    sb.push_str(if comma { ",\n" } else { "\n" });
}

fn field_u(sb: &mut String, k: &str, v: u64, comma: bool, indent: usize) {
    sb.push_str(&" ".repeat(indent));
    sb.push('"');
    sb.push_str(k);
    sb.push_str("\": ");
    sb.push_str(&v.to_string());
    sb.push_str(if comma { ",\n" } else { "\n" });
}

fn field_f(sb: &mut String, k: &str, v: f64, comma: bool, indent: usize) {
    sb.push_str(&" ".repeat(indent));
    sb.push('"');
    sb.push_str(k);
    sb.push_str("\": ");
    sb.push_str(&format!("{v:.6}"));
    sb.push_str(if comma { ",\n" } else { "\n" });
}

fn escape(s: &str) -> String {
    s.replace('\\', "\\\\").replace('"', "\\\"")
}
