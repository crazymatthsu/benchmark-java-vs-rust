from __future__ import annotations

from pathlib import Path

from ..env import EnvInfo
from . import percentiles


class BenchResult:
    def __init__(self, name: str, ops: int, elapsed_ns: int, checksum: int, sorted_samples: list[int]) -> None:
        self.name = name
        self.ops = ops
        self.elapsed_ns = elapsed_ns
        self.throughput_ops_s = 0.0 if elapsed_ns == 0 else ops / (elapsed_ns / 1_000_000_000.0)
        self.checksum = str(checksum)
        self.min_ns = percentiles.min_v(sorted_samples)
        self.p50_ns = percentiles.pct(sorted_samples, 0.50)
        self.p90_ns = percentiles.pct(sorted_samples, 0.90)
        self.p99_ns = percentiles.pct(sorted_samples, 0.99)
        self.p999_ns = percentiles.pct(sorted_samples, 0.999)
        self.max_ns = percentiles.max_v(sorted_samples)
        self.stats: list[tuple[str, int]] = []

    def stat(self, k: str, v: int) -> "BenchResult":
        self.stats.append((k, v))
        return self


def _esc(s: str) -> str:
    return s.replace("\\", "\\\\").replace('"', '\\"')


def write(
    path: Path,
    env: EnvInfo,
    container_runtime: str,
    seed: int,
    ops: int,
    warmup: int,
    symbols: int,
    accounts: int,
    benches: list[BenchResult],
) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    lines = ["{\n"]
    def fs(k, v, comma=True, indent=2):
        lines.append(f'{" " * indent}"{k}": "{_esc(str(v))}"' + (",\n" if comma else "\n"))
    def fi(k, v, comma=True, indent=2):
        lines.append(f'{" " * indent}"{k}": {v}' + (",\n" if comma else "\n"))
    def ff(k, v, comma=True, indent=2):
        lines.append(f'{" " * indent}"{k}": {v:.6f}' + (",\n" if comma else "\n"))

    fi("schema_version", 1)
    fs("language", "python")
    fs("runtime", env.runtime)
    fs("os", env.os)
    fs("arch", env.arch)
    fi("cpus", env.cpus)
    fi("mem_bytes", env.mem_bytes)
    fs("os_pretty", env.os_pretty)
    fs("cpu_model", env.cpu_model)
    fs("container_runtime", container_runtime)
    fs("seed", str(seed))
    fi("ops", ops)
    fi("warmup", warmup)
    fi("symbols", symbols)
    fi("accounts", accounts)
    lines.append('  "benchmarks": [\n')
    for i, b in enumerate(benches):
        comma = i + 1 < len(benches)
        lines.append("    {\n")
        fs("name", b.name, True, 6)
        fi("ops", b.ops, True, 6)
        fi("elapsed_ns", b.elapsed_ns, True, 6)
        ff("throughput_ops_s", b.throughput_ops_s, True, 6)
        fs("checksum", b.checksum, True, 6)
        lines.append('      "latency_ns": {\n')
        fi("min", b.min_ns, True, 8)
        fi("p50", b.p50_ns, True, 8)
        fi("p90", b.p90_ns, True, 8)
        fi("p99", b.p99_ns, True, 8)
        fi("p999", b.p999_ns, True, 8)
        fi("max", b.max_ns, False, 8)
        lines.append("      },\n")
        lines.append('      "stats": {\n')
        for n, (k, v) in enumerate(b.stats):
            fi(k, v, n + 1 < len(b.stats), 8)
        lines.append("      }\n")
        lines.append("    }" + (",\n" if comma else "\n"))
    lines.append("  ]\n")
    lines.append("}\n")
    path.write_text("".join(lines), encoding="utf-8")
