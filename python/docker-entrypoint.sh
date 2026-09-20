#!/bin/sh
set -eu
exec python -m equities_bench \
  --ops "${BENCH_OPS:-1000000}" \
  --warmup "${BENCH_WARMUP:-50000}" \
  --seed "${BENCH_SEED:-0x0C0FFEE123456789}" \
  --symbols "${BENCH_SYMBOLS:-32}" \
  --accounts "${BENCH_ACCOUNTS:-256}" \
  --bench "${BENCH_FILTER:-all}" \
  --output "${OUTPUT:-/results/python.json}" \
  --runtime-name "${CONTAINER_RUNTIME:-unknown}"
