#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

CC="${CROSS_COMPILE:-aarch64-linux-gnu-}gcc"
OBJDUMP="${CROSS_COMPILE:-aarch64-linux-gnu-}objdump"

STARTUP_SRC="${ROOT_DIR}/src/freertos_aarch64_orangepi/src/startup_aarch64.S"
STARTUP_OBJ="${TMP_DIR}/startup.o"
STARTUP_DIS="${TMP_DIR}/startup.dis"

"$CC" \
    -mcpu=cortex-a55 \
    -mgeneral-regs-only \
    -ffreestanding \
    -nostdlib \
    -c "$STARTUP_SRC" \
    -o "$STARTUP_OBJ"

"$OBJDUMP" -dr "$STARTUP_OBJ" > "$STARTUP_DIS"

if ! grep -qE 'mov[[:space:]]+x0, #0x80000000' "$STARTUP_DIS"; then
    echo "FAIL: el2_start does not set HCR_EL2.RW for an AArch64 EL1 return" >&2
    exit 1
fi

if ! grep -qE 'mov[[:space:]]+x0, #0x401' "$STARTUP_DIS"; then
    echo "FAIL: el3_start does not set SCR_EL3.NS|RW for an AArch64 EL1 return" >&2
    exit 1
fi

echo "PASS: startup EL transition configuration requests AArch64 EL1"
