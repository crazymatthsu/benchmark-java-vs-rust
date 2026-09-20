from __future__ import annotations

import os
import platform
import sys
from dataclasses import dataclass


@dataclass
class EnvInfo:
    os: str
    arch: str
    cpus: int
    mem_bytes: int
    os_pretty: str
    cpu_model: str
    runtime: str


def _read(path: str) -> str:
    try:
        with open(path, encoding="utf-8") as f:
            return f.read()
    except OSError:
        return ""


def _find_prefixed(text: str, key: str) -> str | None:
    want = key.lower()
    for line in text.splitlines():
        if ":" not in line:
            continue
        k, v = line.split(":", 1)
        if k.strip().lower() == want:
            return v.strip()
    return None


def collect() -> EnvInfo:
    mem = 0
    meminfo = _read("/proc/meminfo")
    for line in meminfo.splitlines():
        if line.startswith("MemTotal:"):
            digits = "".join(ch for ch in line if ch.isdigit())
            if digits:
                mem = int(digits) * 1024
            break
    osrel = _read("/etc/os-release")
    pretty = _find_prefixed(osrel, "PRETTY_NAME") or f"{platform.system()} {platform.release()}"
    if pretty.startswith('"') and pretty.endswith('"'):
        pretty = pretty[1:-1]
    cpuinfo = _read("/proc/cpuinfo")
    cpu = _find_prefixed(cpuinfo, "model name") or _find_prefixed(cpuinfo, "Hardware")
    if not cpu:
        impl = _find_prefixed(cpuinfo, "CPU implementer")
        part = _find_prefixed(cpuinfo, "CPU part")
        if impl or part:
            cpu = f"implementer {impl or '?'} part {part or '?'}"
    if not cpu:
        cpu = platform.machine()
    return EnvInfo(
        os=sys.platform if sys.platform != "linux" else "linux",
        arch=platform.machine(),
        cpus=os.cpu_count() or 1,
        mem_bytes=mem,
        os_pretty=pretty,
        cpu_model=cpu,
        runtime=f"CPython {sys.version.split()[0]}",
    )
