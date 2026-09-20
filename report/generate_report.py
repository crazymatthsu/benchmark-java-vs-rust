#!/usr/bin/env python3
"""Build HTML/Markdown/PNG comparison reports from language JSON files."""

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

LANGS = [
    ("java", "Java 21", "#2F6FED"),
    ("rust", "Rust", "#E05A00"),
    ("cpp", "C++", "#2A9D8F"),
    ("python", "Python", "#6B4C9A"),
]


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
    fig, ax = plt.subplots(figsize=(11.5, 5.0))
    x = range(len(labels))
    n = len(series)
    width = 0.8 / max(n, 1)
    for i, (name, vals, color) in enumerate(series):
        offs = [xi + (i - (n - 1) / 2) * width for xi in x]
        ax.bar(offs, vals, width=width, label=name, color=color)
    ax.set_xticks(list(x))
    ax.set_xticklabels(labels)
    ax.set_ylabel(ylabel)
    ax.set_title(title)
    ax.legend()
    ax.grid(axis="y", linestyle="--", alpha=0.35)
    fig.tight_layout()
    fig.savefig(path, dpi=140)
    plt.close(fig)


def checksums_match(langs: list[tuple[str, dict]], names: list[str]) -> bool:
    for name in names:
        sums = {bmap(doc)[name]["checksum"] for _, doc in langs if name in bmap(doc)}
        if len(sums) != 1:
            return False
    return True


def geo_vs_java(langs: dict[str, dict], names: list[str]) -> str:
    if "java" not in langs:
        return "n/a (Java result missing)"
    java_b = bmap(langs["java"])
    lines = []
    for key, label, _ in LANGS:
        if key == "java" or key not in langs:
            continue
        other = bmap(langs[key])
        ratios = []
        for n in names:
            if n not in java_b or n not in other:
                continue
            jt = java_b[n]["throughput_ops_s"]
            ot = other[n]["throughput_ops_s"]
            if jt > 0 and ot > 0:
                ratios.append(ot / jt)
        if not ratios:
            continue
        prod = 1.0
        for x in ratios:
            prod *= x
        geo = prod ** (1.0 / len(ratios))
        lines.append(f"{label} / Java: **{geo:.2f}x**")
    return "; ".join(lines) if lines else "n/a"


def write_markdown(path: Path, ctx: dict) -> None:
    langs: dict[str, dict] = ctx["langs"]
    names = ctx["names"]
    present = [(k, lab) for k, lab, _ in LANGS if k in langs]
    header = "| Bench | " + " | ".join(f"{lab} ops/s" for _, lab in present) + " | Checksum |"
    sep = "|---|" + "---:|" * len(present) + "---|"
    rows = []
    for name in names:
        cells = [f"`{name}`"]
        sums = []
        for key, _ in present:
            b = bmap(langs[key]).get(name)
            if b:
                cells.append(fmt_num(b["throughput_ops_s"]))
                sums.append(b["checksum"])
            else:
                cells.append("—")
        match = "yes" if sums and len(set(sums)) == 1 else "NO"
        rows.append("| " + " | ".join(cells) + f" | {match} |")

    env_header = "| | " + " | ".join(lab for _, lab in present) + " |"
    env_sep = "|---|" + "|".join(["---"] * len(present)) + "|"
    def env_row(label, field, same_note=None):
        vals = []
        for key, _ in present:
            vals.append(str(langs[key].get(field, "")))
        if same_note and len(set(vals)) == 1:
            return f"| {label} | " + " | ".join([same_note] + [""] * (len(present) - 1)) + " |"
        return f"| {label} | " + " | ".join(vals) + " |"

    spec = ctx["spec"]
    if len(spec) > 4000:
        spec = spec[:4000] + "\n\n_(truncated; see benches/SPEC.md)_"
    labels = [lab for _, lab in present]
    md = f"""# Equities language benchmark report

Generated **{ctx["generated"]}** UTC. Languages: {", ".join(labels)}.

## Environment

{env_header}
{env_sep}
{env_row("Runtime", "runtime")}
{env_row("OS", "os_pretty")}
{env_row("Arch", "arch")}
{env_row("CPUs", "cpus")}
{env_row("Container", "container_runtime")}
{env_row("CPU", "cpu_model")}

Host (from runner): `{ctx["meta"].get("engine", "?")} {ctx["meta"].get("engine_version", "")}` on `{ctx["meta"].get("host_os", "?")}/{ctx["meta"].get("host_arch", "?")}`.

Checksums **{"MATCH" if ctx["all_match"] else "MISMATCH — do not trust the comparison"}**.

## Throughput and latency

![Throughput](throughput.png)

![p50 latency](latency_p50.png)

![p99 latency](latency_p99.png)

{header}
{sep}
{os.linesep.join(rows)}

Geometric-mean throughput vs Java: {ctx["geo"]}.

## How this run was produced

1. Linux containers for each language (Temurin 21, release Rust, g++ -O3 -flto, CPython 3.12).
2. Same seed, op count, symbols, accounts, and algorithms (`benches/SPEC.md`).
3. Warmup on a throwaway instance, then one measured pass with per-op timers.
4. Single-threaded. This is a language/runtime comparison, not a scaling study.
5. Command: `{ctx["meta"].get("command", "./run-compose.sh")}`

## How to rerun

```bash
./run-compose.sh          # 1,000,000 ops (default)
./run-compose.sh --quick  # 100,000 ops smoke run
./run-compose.sh --full   # 2,000,000 ops
./run-compose.sh --cpp-only --python-only
```

Works with Docker or Podman on macOS, Linux, and Windows Git Bash.

## Methodology excerpt

```
{spec}
```

A p50 of 0 ns means the operation was faster than the container clock (often ~40 ns).
Use throughput for those benches.

These numbers are for this hardware, this container runtime, and this workload.
They are not a universal ranking of the languages.
"""
    path.write_text(md, encoding="utf-8")


def write_html(path: Path, ctx: dict, img_dir: Path) -> None:
    tput = png_b64(img_dir / "throughput.png")
    p50 = png_b64(img_dir / "latency_p50.png")
    p99 = png_b64(img_dir / "latency_p99.png")
    langs: dict[str, dict] = ctx["langs"]
    names = ctx["names"]
    present = [(k, lab) for k, lab, _ in LANGS if k in langs]
    banner = (
        '<div class="ok">Checksums match — all engines did the same work.</div>'
        if ctx["all_match"]
        else '<div class="bad">Checksum mismatch — implementations diverged; do not trust timings.</div>'
    )
    env_cells = "".join(f"<th>{lab}</th>" for _, lab in present)
    def env_tr(label, field):
        tds = "".join(f"<td>{langs[k].get(field, '')}</td>" for k, _ in present)
        return f"<tr><td>{label}</td>{tds}</tr>"
    head = "<th>Bench</th>" + "".join(f"<th>{lab} ops/s</th>" for _, lab in present) + "<th>Checksum</th>"
    rows = []
    for name in names:
        tds = [f"<td><code>{name}</code></td>"]
        sums = []
        for key, _ in present:
            b = bmap(langs[key]).get(name)
            if b:
                tds.append(f"<td class='n'>{fmt_num(b['throughput_ops_s'])}</td>")
                sums.append(b["checksum"])
            else:
                tds.append("<td>—</td>")
        match = "yes" if sums and len(set(sums)) == 1 else "NO"
        tds.append(f"<td>{match}</td>")
        rows.append("<tr>" + "".join(tds) + "</tr>")
    html = f"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8"/>
<title>Equities language benchmarks</title>
<style>
 body {{ font-family: ui-sans-serif, system-ui, sans-serif; margin: 2rem auto; max-width: 1080px;
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
<h1>Equities language benchmarks</h1>
<p class="muted">Generated {ctx["generated"]} UTC</p>
{banner}
<h2>Environment</h2>
<table>
<tr><th></th>{env_cells}</tr>
{env_tr("Runtime", "runtime")}
{env_tr("OS", "os_pretty")}
{env_tr("Arch", "arch")}
{env_tr("CPUs", "cpus")}
{env_tr("Container", "container_runtime")}
{env_tr("CPU", "cpu_model")}
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
<tr>{head}</tr>
{''.join(rows)}
</table>
<p>Geometric-mean throughput vs Java: {ctx["geo"]}.</p>
<h2>How to rerun</h2>
<pre>./run-compose.sh
./run-compose.sh --quick
./run-compose.sh --full</pre>
<p>Works with Docker or Podman on macOS, Linux, and Windows Git Bash.</p>
<p class="muted">These numbers are for this hardware and workload, not a universal language ranking.
A p50 of 0 ns means the op was faster than the container clock; use throughput for those benches.</p>
</body>
</html>
"""
    path.write_text(html, encoding="utf-8")


def main() -> int:
    langs: dict[str, dict] = {}
    paths: dict[str, Path] = {}
    for key, _, _ in LANGS:
        p = RESULTS / f"{key}.json"
        if p.exists():
            langs[key] = load(p)
            paths[key] = p
    if len(langs) < 2:
        print(f"need at least two of java/rust/cpp/python JSON under {RESULTS}", file=sys.stderr)
        return 2
    meta = {}
    meta_path = RESULTS / "meta.json"
    if meta_path.exists():
        meta = load(meta_path)

    name_sets = [set(bmap(doc)) for doc in langs.values()]
    names = [n for n in next(iter(name_sets)) if all(n in s for s in name_sets)]
    if not names:
        print("no overlapping benchmarks", file=sys.stderr)
        return 2
    all_match = checksums_match([(k, langs[k]) for k in langs], names)
    geo = geo_vs_java(langs, names)
    spec = SPEC.read_text(encoding="utf-8") if SPEC.exists() else "(spec not mounted)"
    generated = datetime.now(timezone.utc).strftime("%Y-%m-%d %H:%M:%S")
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    out = REPORTS / stamp
    latest = REPORTS / "latest"
    out.mkdir(parents=True, exist_ok=True)

    present_series_meta = [(k, lab, color) for k, lab, color in LANGS if k in langs]
    grouped_bar(
        out / "throughput.png",
        names,
        [(lab, [bmap(langs[k])[n]["throughput_ops_s"] for n in names], color) for k, lab, color in present_series_meta],
        "ops / second",
        "Throughput (higher is better)",
    )
    grouped_bar(
        out / "latency_p50.png",
        names,
        [(lab, [bmap(langs[k])[n]["latency_ns"]["p50"] for n in names], color) for k, lab, color in present_series_meta],
        "nanoseconds",
        "Latency p50 (lower is better)",
    )
    grouped_bar(
        out / "latency_p99.png",
        names,
        [(lab, [bmap(langs[k])[n]["latency_ns"]["p99"] for n in names], color) for k, lab, color in present_series_meta],
        "nanoseconds",
        "Latency p99 (lower is better)",
    )

    ctx = {
        "langs": langs,
        "names": names,
        "all_match": all_match,
        "geo": geo,
        "spec": spec,
        "generated": generated,
        "meta": meta,
    }
    write_markdown(out / "REPORT.md", ctx)
    write_html(out / "index.html", ctx, out)
    for key, p in paths.items():
        shutil.copy2(p, out / f"{key}.json")
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
    print("languages", ",".join(langs))
    print("checksums", "MATCH" if all_match else "MISMATCH")
    return 0 if all_match else 1


if __name__ == "__main__":
    sys.exit(main())
