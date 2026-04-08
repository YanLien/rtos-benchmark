#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
TARGETS=(
    "${ROOT_DIR}/src/freertos_aarch64_orangepi/src/timer/bench_generic_timer.c"
    "${ROOT_DIR}/src/freertos_aarch64_qemu/src/timer/bench_generic_timer.c"
)

for target in "${TARGETS[@]}"; do
    if ! grep -Eq 'BENCH_TIMER_IRQ' "$target"; then
        echo "FAIL: $target does not use a dedicated benchmark timer IRQ" >&2
        exit 1
    fi

    if ! grep -Eq 'cntv_tval_el0|cntv_ctl_el0|cntvct_el0' "$target"; then
        echo "FAIL: $target does not program the virtual timer registers for the benchmark IRQ path" >&2
        exit 1
    fi
done

echo "PASS: benchmark timer path uses a dedicated virtual timer IRQ"
