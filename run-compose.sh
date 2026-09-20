#!/usr/bin/env bash
# Portable runner for macOS, Linux, and Windows Git Bash.
# Uses Docker Compose when a working Docker engine is present, otherwise Podman.
set -euo pipefail

if [ -n "${MSYSTEM:-}" ] || [ -n "${MSYS:-}" ]; then
  export MSYS_NO_PATHCONV=1
  export MSYS2_ARG_CONV_EXCL='*'
fi

ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

have() { command -v "$1" >/dev/null 2>&1; }

usage() {
  cat <<'EOF'
Usage: ./run-compose.sh [options]

  --quick          100,000 ops (smoke)
  --full           2,000,000 ops
  --ops N          override op count
  --warmup N       override warmup ops
  --java-only      skip rust
  --rust-only      skip java
  --no-report      skip report container
  --build-only     build images and exit
  -h, --help

Requires Docker or Podman. Linux images, sequential run (Java heap is ~1g).
Reports: reports/latest/index.html
EOF
}

MODE="default"
BENCH_OPS="${BENCH_OPS:-1000000}"
BENCH_WARMUP="${BENCH_WARMUP:-50000}"
RUN_JAVA=1
RUN_RUST=1
RUN_REPORT=1
BUILD_ONLY=0

while [ $# -gt 0 ]; do
  case "$1" in
    -h|--help) usage; exit 0 ;;
    --quick) MODE="quick"; BENCH_OPS=100000; BENCH_WARMUP=10000 ;;
    --full) MODE="full"; BENCH_OPS=2000000; BENCH_WARMUP=100000 ;;
    --ops) BENCH_OPS="$2"; shift ;;
    --warmup) BENCH_WARMUP="$2"; shift ;;
    --java-only) RUN_RUST=0 ;;
    --rust-only) RUN_JAVA=0 ;;
    --no-report) RUN_REPORT=0 ;;
    --build-only) BUILD_ONLY=1 ;;
    *) echo "unknown option: $1" >&2; usage; exit 1 ;;
  esac
  shift
done

ENGINE=""
COMPOSE=()

if have docker && docker info >/dev/null 2>&1; then
  ENGINE=docker
  if docker compose version >/dev/null 2>&1; then
    COMPOSE=(docker compose)
  elif have docker-compose; then
    COMPOSE=(docker-compose)
  fi
fi

if [ -z "$ENGINE" ] && have podman; then
  ENGINE=podman
  if ! podman info >/dev/null 2>&1; then
    echo "starting podman machine..."
    podman machine start >/dev/null 2>&1 || true
  fi
  if podman compose version >/dev/null 2>&1; then
    COMPOSE=(podman compose)
  elif have podman-compose; then
    COMPOSE=(podman-compose)
  fi
fi

if [ -z "$ENGINE" ] || [ ${#COMPOSE[@]} -eq 0 ]; then
  echo "Need a working Docker or Podman compose client." >&2
  exit 1
fi

ENGINE_VER="$("$ENGINE" --version 2>/dev/null | head -1 || true)"
export BENCH_OPS BENCH_WARMUP
export BENCH_SEED="${BENCH_SEED:-0x0C0FFEE123456789}"
export BENCH_SYMBOLS="${BENCH_SYMBOLS:-32}"
export BENCH_ACCOUNTS="${BENCH_ACCOUNTS:-256}"
export BENCH_FILTER="${BENCH_FILTER:-all}"
export CONTAINER_RUNTIME="$ENGINE"

mkdir -p "$ROOT/results" "$ROOT/reports"

FULL_CMD="./run-compose.sh"
if [ "$MODE" = "quick" ]; then FULL_CMD="./run-compose.sh --quick"; fi
if [ "$MODE" = "full" ]; then FULL_CMD="./run-compose.sh --full"; fi

cat > "$ROOT/results/meta.json" <<EOF
{
  "engine": "$ENGINE",
  "engine_version": "$ENGINE_VER",
  "host_os": "$(uname -s)",
  "host_arch": "$(uname -m)",
  "command": "$FULL_CMD",
  "started_utc": "$(date -u +%Y-%m-%dT%H:%M:%SZ)",
  "ops": $BENCH_OPS,
  "warmup": $BENCH_WARMUP,
  "mode": "$MODE"
}
EOF

echo "engine: $ENGINE ($ENGINE_VER)"
echo "ops=$BENCH_OPS warmup=$BENCH_WARMUP seed=$BENCH_SEED"

echo "==> building images"
"${COMPOSE[@]}" -f "$ROOT/docker-compose.yml" build rust-bench java-bench report

if [ "$BUILD_ONLY" -eq 1 ]; then
  echo "build complete"
  exit 0
fi

# Sequential: the Podman VM on this project is 6 GiB; do not run JVM + Rust together.
if [ "$RUN_RUST" -eq 1 ]; then
  echo "==> rust-bench"
  "${COMPOSE[@]}" -f "$ROOT/docker-compose.yml" run --rm -T rust-bench
fi
if [ "$RUN_JAVA" -eq 1 ]; then
  echo "==> java-bench"
  "${COMPOSE[@]}" -f "$ROOT/docker-compose.yml" run --rm -T java-bench
fi
if [ "$RUN_REPORT" -eq 1 ] && [ "$RUN_JAVA" -eq 1 ] && [ "$RUN_RUST" -eq 1 ]; then
  echo "==> report"
  "${COMPOSE[@]}" -f "$ROOT/docker-compose.yml" run --rm -T report
  echo "report: $ROOT/reports/latest/index.html"
fi

echo "done."
