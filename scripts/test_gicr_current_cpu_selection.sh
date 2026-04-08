#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
TARGETS=(
    "${ROOT_DIR}/src/freertos_aarch64_orangepi/src/board/gicv3.c"
    "${ROOT_DIR}/src/freertos_aarch64_qemu/src/board/gicv3.c"
)

for target in "${TARGETS[@]}"; do
    if ! grep -Eq 'mpidr_el1' "$target"; then
        echo "FAIL: $target does not read MPIDR_EL1 for per-CPU redistributor selection" >&2
        exit 1
    fi

    if ! grep -Eq 'GICR_TYPER' "$target"; then
        echo "FAIL: $target does not inspect GICR_TYPER while selecting a redistributor frame" >&2
        exit 1
    fi
done

echo "PASS: GIC drivers derive the redistributor frame from the current CPU affinity"
