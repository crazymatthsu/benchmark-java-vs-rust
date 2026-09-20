#!/usr/bin/env python3
"""Build HTML/Markdown/PNG comparison reports from java.json and rust.json."""

from __future__ import annotations

import base64
import json
import os
import shutil
import sys
from datetime import datetime, timezone
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402


RESULTS = Path(os.environ.get("RESULTS_DIR", "/results"))
REPORTS = Path(os.environ.get("REPORTS_DIR", "/reports"))
SPEC = Path(os.environ.get("SPEC_PATH", "/spec/SPEC.md"))


def load(path: Path) -> dict:
    with path.open() as f:
        return json.load(f)


def bmap(doc: dict) -> dict:
    return {b["name"]: b for b in doc.get("benchmarks", [])}


def fmt_num(n: float) -> str:
    if n >= 1_000_000:
        return f"{n / 1_000_000:.2f} M"
    if n >= 1_000:
        return f"{n / 1_000:.2f} k"
    return f"{n:.2f}"


def png_b64(path: Path) -> str:
    return base64.b64encode(path.read_bytes()).decode("ascii")


def grouped_bar(path: Path, labels, series, ylabel, title):
    fig, ax = plt.subplots(figsize=(10.5, 4.8))
    x = range(len(labels))
    n = len(series)
    width = 0.8 / max(n, 1)
    colors = ["#2F6FED", "#E05A00", "#2A9D8F"]
    for i, (name, vals) in enumerate(series):
        offs = [xi + (i - (n - 1) / 2) * width for xi in x]
        ax.bar(offs, vals, width=width, label=name, color=colors[i % len(colors)])
    ax.set_xticks(list(x))
    ax.set_xticklabels(labels)
    ax.set_ylabel(ylabel)
    ax.set_title(title)
    ax.legend()
    ax.grid(axis="y", linestyle="--", alpha=0.35)
    fig.tight_layout()
    fig.savefig(path, dpi=140)
    plt.close(fig)


def write_markdown(path: Path, ctx: dict) -> None:
    rows = []
    for name in ctx["names"]:
        j, r = ctx["java_b"][name], ctx["rust_b"][name]
        jt, rt = j["throughput_ops_s"], r["throughput_ops_s"]
        speed = (rt / jt) if jt else 0
        match = "yes" if j["checksum"] == r["checksum"] else "NO"
        rows.append(
            f"| `{name}` | {fmt_num(jt)} | {fmt_num(rt)} | {speed:.2f}x | "
            f"{j['latency_ns']['p50']} | {r['latency_ns']['p50']} | "
            f"{j['latency_ns']['p99']} | {r['latency_ns']['p99']} | {match} |"
        )
    spec = ctx["spec"]
    if len(spec) > 4000:
        spec = spec[:4000] + "\n\n_(truncated; see benches/SPEC.md)_"
    md = f"""# Java 21 vs Rust — equities benchmark report

Generated **{ctx["generated"]}** UTC.

## Environment

| | Java | Rust |
|---|---|---|
| Runtime | {ctx["java"].get("runtime", "")} | {ctx["rust"].get("runtime", "")} |
| OS | {ctx["java"].get("os_pretty", "")} | {ctx["rust"].get("os_pretty", "")} |
| Arch | {ctx["java"].get("arch", "")} | {ctx["rust"].get("arch", "")} |
| CPUs | {ctx["java"].get("cpus", "")} | {ctx["rust"].get("cpus", "")} |
| Container | {ctx["java"].get("container_runtime", "")} | {ctx["rust"].get("container_runtime", "")} |
| CPU | {ctx["java"].get("cpu_model", "")} | {ctx["rust"].get("cpu_model", "")} |
| Ops / warmup | {ctx["java"].get("ops")} / {ctx["java"].get("warmup")} | same |
| Seed | {ctx["java"].get("seed")} | {ctx["rust"].get("seed")} |

Host (from runner): `{ctx["meta"].get("engine", "?")} {ctx["meta"].get("engine_version", "")}` on `{ctx["meta"].get("host_os", "?")}/{ctx["meta"].get("host_arch", "?")}`.

Checksums **{"MATCH" if ctx["all_match"] else "MISMATCH — do not trust the comparison"}**.

## Throughput and latency

![Throughput](throughput.png)

![p50 latency](latency_p50.png)

![p99 latency](latency_p99.png)

| Bench | Java ops/s | Rust ops/s | Rust/Java | Java p50 ns | Rust p50 ns | Java p99 ns | Rust p99 ns | Checksum |
|---|---:|---:|---:|---:|---:|---:|---:|---|
{os.linesep.join(rows)}

Geometric-mean throughput speedup (Rust / Java): **{ctx["geo"]:.2f}x**.

## How this run was produced

1. Linux containers (Temurin 21 JRE and Debian slim + release Rust).
2. Same seed, op count, symbols, accounts, and algorithms (`benches/SPEC.md`).
3. Warmup on a throwaway instance, then one measured pass with per-op timers.
4. Single-threaded. This is a language/runtime comparison, not a scaling study.
5. Command: `{ctx["meta"].get("command", "./run-compose.sh")}`

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
{spec}
```

A p50 of 0 ns means the operation was faster than the container clock (often ~40 ns).
Use throughput for those benches.

These numbers are for this hardware, this container runtime, and this workload.
They are not a universal ranking of the two languages.
"""
    path.write_text(md, encoding="utf-8")


def write_html(path: Path, ctx: dict, img_dir: Path) -> None:
    tput = png_b64(img_dir / "throughput.png")
    p50 = png_b64(img_dir / "latency_p50.png")
    p99 = png_b64(img_dir / "latency_p99.png")
    banner = (
        '<div class="ok">Checksums match — both engines did the same work.</div>'
        if ctx["all_match"]
        else '<div class="bad">Checksum mismatch — implementations diverged; do not trust timings.</div>'
    )
    rows = []
    for name in ctx["names"]:
        j, r = ctx["java_b"][name], ctx["rust_b"][name]
        jt, rt = j["throughput_ops_s"], r["throughput_ops_s"]
        speed = (rt / jt) if jt else 0
        match = "yes" if j["checksum"] == r["checksum"] else "NO"
        rows.append(
            "<tr>"
            f"<td><code>{name}</code></td>"
            f"<td class='n'>{fmt_num(jt)}</td>"
            f"<td class='n'>{fmt_num(rt)}</td>"
            f"<td class='n'>{speed:.2f}x</td>"
            f"<td class='n'>{j['latency_ns']['p50']}</td>"
            f"<td class='n'>{r['latency_ns']['p50']}</td>"
            f"<td class='n'>{j['latency_ns']['p99']}</td>"
            f"<td class='n'>{r['latency_ns']['p99']}</td>"
            f"<td>{match}</td>"
            "</tr>"
        )
    html = f"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8"/>
<title>Java 21 vs Rust — equities benchmarks</title>
<style>
 body {{ font-family: ui-sans-serif, system-ui, sans-serif; margin: 2rem auto; max-width: 980px;
        color: #14213d; background: #f7f5f2; }}
 h1,h2 {{ font-weight: 650; }}
 table {{ border-collapse: collapse; width: 100%; background: #fff; }}
 th, td {{ border: 1px solid #ddd; padding: 0.45rem 0.6rem; text-align: left; }}
 th {{ background: #14213d; color: #fff; }}
 td.n {{ text-align: right; font-variant-numeric: tabular-nums; }}
 img {{ max-width: 100%; background: #fff; border: 1px solid #eee; }}
 .ok {{ background: #e3f6e8; padding: 0.8rem 1rem; border-left: 4px solid #2a9d8f; }}
 .bad {{ background: #fde8e8; padding: 0.8rem 1rem; border-left: 4px solid #c1121f; }}
 code, pre {{ font-family: ui-monospace, SFMono-Regular, Menlo, monospace; }}
 pre {{ background: #fff; padding: 1rem; overflow: auto; border: 1px solid #eee; }}
 .muted {{ color: #555; }}
</style>
</head>
<body>
<h1>Java 21 vs Rust — equities benchmarks</h1>
<p class="muted">Generated {ctx["generated"]} UTC</p>
{banner}
<h2>Environment</h2>
<table>
<tr><th></th><th>Java</th><th>Rust</th></tr>
<tr><td>Runtime</td><td>{ctx["java"].get("runtime","")}</td><td>{ctx["rust"].get("runtime","")}</td></tr>
<tr><td>OS</td><td>{ctx["java"].get("os_pretty","")}</td><td>{ctx["rust"].get("os_pretty","")}</td></tr>
<tr><td>Arch</td><td>{ctx["java"].get("arch","")}</td><td>{ctx["rust"].get("arch","")}</td></tr>
<tr><td>CPUs</td><td>{ctx["java"].get("cpus","")}</td><td>{ctx["rust"].get("cpus","")}</td></tr>
<tr><td>Container</td><td>{ctx["java"].get("container_runtime","")}</td><td>{ctx["rust"].get("container_runtime","")}</td></tr>
<tr><td>CPU</td><td>{ctx["java"].get("cpu_model","")}</td><td>{ctx["rust"].get("cpu_model","")}</td></tr>
<tr><td>Ops / warmup</td><td>{ctx["java"].get("ops")} / {ctx["java"].get("warmup")}</td><td>same</td></tr>
</table>
<p>Host: <code>{ctx["meta"].get("engine","?")} {ctx["meta"].get("engine_version","")}</code>
on <code>{ctx["meta"].get("host_os","?")}/{ctx["meta"].get("host_arch","?")}</code></p>
<h2>Throughput</h2>
<img alt="throughput" src="data:image/png;base64,{tput}"/>
<h2>Latency p50</h2>
<img alt="p50" src="data:image/png;base64,{p50}"/>
<h2>Latency p99</h2>
<img alt="p99" src="data:image/png;base64,{p99}"/>
<h2>Summary</h2>
<table>
<tr><th>Bench</th><th>Java ops/s</th><th>Rust ops/s</th><th>Rust/Java</th>
<th>Java p50</th><th>Rust p50</th><th>Java p99</th><th>Rust p99</th><th>Checksum</th></tr>
{''.join(rows)}
</table>
<p>Geometric-mean throughput speedup (Rust / Java): <strong>{ctx["geo"]:.2f}x</strong>.</p>
<h2>How to rerun</h2>
<pre>./run-compose.sh
./run-compose.sh --quick
./run-compose.sh --full</pre>
<p>Works with Docker or Podman on macOS, Linux, and Windows Git Bash. See README.md and benches/SPEC.md.</p>
<p class="muted">These numbers are for this hardware and workload, not a universal language ranking.
A p50 of 0 ns means the op was faster than the container clock (often ~40 ns on this VM);
use throughput for those benches.</p>
</body>
</html>
"""
    path.write_text(html, encoding="utf-8")


def main() -> int:
    java_path = RESULTS / "java.json"
    rust_path = RESULTS / "rust.json"
    if not java_path.exists() or not rust_path.exists():
        print(f"missing results under {RESULTS}", file=sys.stderr)
        return 2
    java = load(java_path)
    rust = load(rust_path)
    meta = {}
    meta_path = RESULTS / "meta.json"
    if meta_path.exists():
        meta = load(meta_path)
    java_b, rust_b = bmap(java), bmap(rust)
    names = [n for n in java_b if n in rust_b]
    if not names:
        print("no overlapping benchmarks", file=sys.stderr)
        return 2
    all_match = all(java_b[n]["checksum"] == rust_b[n]["checksum"] for n in names)
    ratios = []
    for n in names:
        jt = java_b[n]["throughput_ops_s"]
        rt = rust_b[n]["throughput_ops_s"]
        if jt > 0 and rt > 0:
            ratios.append(rt / jt)
    geo = 1.0
    if ratios:
        prod = 1.0
        for x in ratios:
            prod *= x
        geo = prod ** (1.0 / len(ratios))
    spec = SPEC.read_text(encoding="utf-8") if SPEC.exists() else "(spec not mounted)"
    generated = datetime.now(timezone.utc).strftime("%Y-%m-%d %H:%M:%S")
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    out = REPORTS / stamp
    latest = REPORTS / "latest"
    out.mkdir(parents=True, exist_ok=True)

    grouped_bar(
        out / "throughput.png",
        names,
        [
            ("Java 21", [java_b[n]["throughput_ops_s"] for n in names]),
            ("Rust", [rust_b[n]["throughput_ops_s"] for n in names]),
        ],
        "ops / second",
        "Throughput (higher is better)",
    )
    grouped_bar(
        out / "latency_p50.png",
        names,
        [
            ("Java 21", [java_b[n]["latency_ns"]["p50"] for n in names]),
            ("Rust", [rust_b[n]["latency_ns"]["p50"] for n in names]),
        ],
        "nanoseconds",
        "Latency p50 (lower is better)",
    )
    grouped_bar(
        out / "latency_p99.png",
        names,
        [
            ("Java 21", [java_b[n]["latency_ns"]["p99"] for n in names]),
            ("Rust", [rust_b[n]["latency_ns"]["p99"] for n in names]),
        ],
        "nanoseconds",
        "Latency p99 (lower is better)",
    )

    ctx = {
        "java": java,
        "rust": rust,
        "java_b": java_b,
        "rust_b": rust_b,
        "names": names,
        "all_match": all_match,
        "geo": geo,
        "spec": spec,
        "generated": generated,
        "meta": meta,
    }
    write_markdown(out / "REPORT.md", ctx)
    write_html(out / "index.html", ctx, out)
    shutil.copy2(java_path, out / "java.json")
    shutil.copy2(rust_path, out / "rust.json")
    if meta_path.exists():
        shutil.copy2(meta_path, out / "meta.json")

    if latest.exists() or latest.is_symlink():
        if latest.is_dir() and not latest.is_symlink():
            shutil.rmtree(latest)
        else:
            latest.unlink()
    shutil.copytree(out, latest)

    print(f"wrote {out}")
    print(f"latest {latest / 'index.html'}")
    print("checksums", "MATCH" if all_match else "MISMATCH")
    return 0 if all_match else 1


if __name__ == "__main__":
    sys.exit(main())
