#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

CC="${CROSS_COMPILE:-aarch64-linux-gnu-}gcc"
OBJDUMP="${CROSS_COMPILE:-aarch64-linux-gnu-}objdump"

SRC="${ROOT_DIR}/src/freertos_aarch64_orangepi/src/board/gicv3.c"
OBJ="${TMP_DIR}/gicv3.o"
DIS="${TMP_DIR}/gicv3.dis"

"$CC" \
    -mcpu=cortex-a55 \
    -mgeneral-regs-only \
    -ffreestanding \
    -nostdlib \
    -fno-pic \
    -fno-pie \
    -DBOARD_ORANGE_PI_5 \
    -I"${ROOT_DIR}/src/freertos_aarch64_orangepi/src" \
    -I"${ROOT_DIR}/src/freertos_aarch64_orangepi/src/board" \
    -c "$SRC" \
    -o "$OBJ"

"$OBJDUMP" -dr "$OBJ" > "$DIS"

FUNC_BODY="$(sed -n '/<gicv3_set_priority>/,/<gicv3_acknowledge_irq>/p' "$DIS")"

if printf '%s\n' "$FUNC_BODY" | grep -Eq '\<strb\>.*\[(x[0-9]+|xzr)\]'; then
    echo "FAIL: gicv3_set_priority still emits MMIO byte stores; use 32-bit read-modify-write for GIC priority registers." >&2
    exit 1
fi

echo "PASS: gicv3_set_priority avoids byte stores"
