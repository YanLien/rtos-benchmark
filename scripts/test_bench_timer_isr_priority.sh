#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
TARGETS=(
    "${ROOT_DIR}/src/freertos_aarch64_orangepi/src/timer/bench_generic_timer.c"
    "${ROOT_DIR}/src/freertos_aarch64_qemu/src/timer/bench_generic_timer.c"
)

for target in "${TARGETS[@]}"; do
    if ! grep -Eq 'gicv3_set_priority\(TIMER_EL1_IRQ, configMAX_API_CALL_INTERRUPT_PRIORITY\);' "$target"; then
        echo "FAIL: $target does not program the benchmark timer IRQ to configMAX_API_CALL_INTERRUPT_PRIORITY" >&2
        exit 1
    fi
done

echo "PASS: benchmark timer IRQ priority matches configMAX_API_CALL_INTERRUPT_PRIORITY"
