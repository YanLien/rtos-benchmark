#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
TARGET="${ROOT_DIR}/src/freertos_aarch64_orangepi/src/timer/bench_generic_timer.c"

if ! grep -Eq '^void bench_exit_timer_isr\(void\)' "$TARGET"; then
    echo "FAIL: Orange Pi target does not override bench_exit_timer_isr(), so the interrupt benchmark reuses the default tick-exit path." >&2
    exit 1
fi

echo "PASS: Orange Pi target overrides bench_exit_timer_isr()"
