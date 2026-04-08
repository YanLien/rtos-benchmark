#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
TARGETS=(
    "${ROOT_DIR}/src/freertos_aarch64_orangepi/build.sh"
    "${ROOT_DIR}/src/freertos_aarch64_qemu/build.sh"
)

for target in "${TARGETS[@]}"; do
    if ! grep -Eq 'COMMON_SRC=.*common' "$target"; then
        echo "FAIL: $target does not define COMMON_SRC for shared benchmark sources" >&2
        exit 1
    fi

    if ! grep -Eq 'COMPILE_C "\$\{COMMON_SRC\}/bench_utils\.c"' "$target"; then
        echo "FAIL: $target does not compile bench_utils.c from src/common" >&2
        exit 1
    fi

    if ! grep -Eq 'COMPILE_C "\$\{COMMON_SRC\}/bench_all\.c"' "$target"; then
        echo "FAIL: $target does not compile bench_all.c from src/common" >&2
        exit 1
    fi

    if ! grep -Eq 'COMPILE_C "\$\{COMMON_SRC\}/bench_\$\{test\}_test\.c"' "$target"; then
        echo "FAIL: $target does not compile benchmark tests from src/common" >&2
        exit 1
    fi

    if grep -Eq '\$\{SRC\}/tests/bench_\$\{test\}_test\.c' "$target"; then
        echo "FAIL: $target still compiles target-local benchmark tests" >&2
        exit 1
    fi
done

echo "PASS: AArch64 build scripts use shared benchmark sources from src/common"
