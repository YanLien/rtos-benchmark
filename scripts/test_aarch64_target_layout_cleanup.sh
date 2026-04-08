#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
TARGETS=(
    "${ROOT_DIR}/src/freertos_aarch64_orangepi/build.sh"
    "${ROOT_DIR}/src/freertos_aarch64_qemu/build.sh"
)

for dir in \
    "${ROOT_DIR}/src/freertos_aarch64_orangepi/src" \
    "${ROOT_DIR}/src/freertos_aarch64_qemu/src"; do
    if find "$dir" -maxdepth 2 \( -name 'bench_all.c' -o -name 'bench_utils.c' -o -path '*/tests' \) | grep -q .; then
        echo "FAIL: $dir still contains duplicated benchmark sources or a tests directory" >&2
        exit 1
    fi
done

for target in "${TARGETS[@]}"; do
    if grep -Eq 'if \[ -f "\$\{COMMON_SRC\}/bench_\$\{test\}_test\.c" \]' "$target"; then
        echo "FAIL: $target still keeps obsolete file-existence guards for shared benchmark tests" >&2
        exit 1
    fi
done

echo "PASS: AArch64 target trees are cleaned up and build scripts no longer keep target-local test fallbacks"
