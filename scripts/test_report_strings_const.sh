#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
TARGETS=(
    "${ROOT_DIR}/src/freertos_aarch64_orangepi/src/tests/bench_mutex_lock_unlock_test.c"
    "${ROOT_DIR}/src/common/bench_mutex_lock_unlock_test.c"
)

for target in "${TARGETS[@]}"; do
    if ! grep -Eq 'static const char \* const report_strings\[NUM_TIMES\]' "$target"; then
        echo "FAIL: $target still declares report_strings as writable pointers" >&2
        exit 1
    fi
done

echo "PASS: report_strings declarations are read-only pointer arrays"
