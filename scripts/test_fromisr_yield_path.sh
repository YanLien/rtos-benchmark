#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
TARGETS=(
    "${ROOT_DIR}/src/freertos_aarch64_orangepi/src/bench_porting_layer_aarch64.c"
    "${ROOT_DIR}/src/freertos_aarch64_qemu/src/bench_porting_layer_aarch64.c"
)

for target in "${TARGETS[@]}"; do
    if ! grep -Eq 'xSemaphoreGiveFromISR\(.*&xHigherPriorityTaskWoken\)' "$target"; then
        echo "FAIL: $target does not capture xHigherPriorityTaskWoken in bench_sem_give_from_isr()" >&2
        exit 1
    fi

    if ! grep -Eq 'portYIELD_FROM_ISR\(xHigherPriorityTaskWoken\)' "$target"; then
        echo "FAIL: $target does not request a yield from bench_sem_give_from_isr()" >&2
        exit 1
    fi
done

echo "PASS: FromISR semaphore give paths capture wakeup state and request a yield"
