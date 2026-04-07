#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Build FreeRTOS rtos-benchmark for RK3588 Orange Pi 5 Plus (Cortex-A55)
#
# Usage:
#   bash build.sh [OPTIONS]
#
# Options:
#   -k, --kernel PATH    FreeRTOS-Kernel source path
#   -o, --output PATH    Build output directory (default: build)
#   -c, --clean          Clean before build
#   -f, --flash HOST     Flash binary to Orange Pi via scp
#   -h, --help           Show this help
#
# Environment variables:
#   CROSS_COMPILE        Cross compiler prefix (default: aarch64-linux-gnu-)
#   FREERTOS_KERNEL_PATH FreeRTOS-Kernel source path (overridden by -k)
#   ITERATIONS           Benchmark iterations (default: 10000)
#   CALIBRATION_LOOPS    Calibration loops (default: 10000)

set -e

# ── Defaults ──────────────────────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJ="${SCRIPT_DIR}"
CROSS_COMPILE="${CROSS_COMPILE:-aarch64-linux-gnu-}"
ITERATIONS="${ITERATIONS:-10000}"
CALIBRATION_LOOPS="${CALIBRATION_LOOPS:-10000}"
CC="${CROSS_COMPILE}gcc"
OBJCOPY="${CROSS_COMPILE}objcopy"
SIZE="${CROSS_COMPILE}size"
BD="${PROJ}/build"
KERNEL=""
CLEAN=0
FLASH=""

# ── Parse arguments ───────────────────────────────────────────────────────────
while [[ $# -gt 0 ]]; do
    case $1 in
        -k|--kernel)   KERNEL="$2"; shift 2 ;;
        -o|--output)   BD="$2"; shift 2 ;;
        -c|--clean)    CLEAN=1; shift ;;
        -f|--flash)    FLASH="$2"; shift 2 ;;
        -h|--help)
            sed -n '2,15p' "$0" | sed 's/^# \?//'
            exit 0 ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
done

# ── Resolve FreeRTOS-Kernel path ─────────────────────────────────────────────
if [ -z "$KERNEL" ]; then
    KERNEL="${FREERTOS_KERNEL_PATH:-}"
fi
if [ -z "$KERNEL" ]; then
    for candidate in \
        "${PROJ}/FreeRTOS-Kernel" \
        "${PROJ}/../FreeRTOS-Kernel" \
        "${PROJ}/../../FreeRTOS-Kernel" \
        "${PROJ}/../../../FreeRTOS-Kernel"; do
        if [ -f "${candidate}/include/FreeRTOS.h" ]; then
            KERNEL="$(cd "$candidate" && pwd)"
            break
        fi
    done
fi
if [ -z "$KERNEL" ] || [ ! -f "${KERNEL}/include/FreeRTOS.h" ]; then
    echo "ERROR: FreeRTOS-Kernel not found."
    echo "  Set FREERTOS_KERNEL_PATH or use -k <path>"
    echo "  Expected: <path>/include/FreeRTOS.h"
    exit 1
fi

SRC="${PROJ}/src"

# ── Verify source directory ──────────────────────────────────────────────────
if [ ! -f "${SRC}/bench_porting_layer_aarch64.c" ]; then
    echo "ERROR: Source directory not found: ${SRC}"
    exit 1
fi

# ── Clean ─────────────────────────────────────────────────────────────────────
if [ "$CLEAN" -eq 1 ]; then
    rm -rf "$BD"
fi
mkdir -p "$BD"

# ── Compiler flags ────────────────────────────────────────────────────────────
CF="-mcpu=cortex-a55 -mgeneral-regs-only -ffreestanding -nostdlib"
CF="$CF -Wall -Wno-unused-parameter -Wno-unused-variable"
CF="$CF -include stdbool.h"
CF="$CF -DFREERTOS_AARCH64"
CF="$CF -DBOARD_ORANGE_PI_5"
CF="$CF -DSYS_CLOCK_HW_CYCLES_PER_SEC=1800000000"
CF="$CF -DITERATIONS=${ITERATIONS} -DCALIBRATION_LOOPS=${CALIBRATION_LOOPS}"
CF="$CF -I${PROJ}/h"
CF="$CF -I${SRC}"
CF="$CF -I${SRC}/board"
CF="$CF -I${SRC}/port"
CF="$CF -I${KERNEL}/include"

cd "$PROJ"

# ── Compile helper ────────────────────────────────────────────────────────────
COMPILE_C() {
    local src="$1"
    local obj="$2"
    if ! $CC $CF -c "$src" -o "$obj" 2>"${obj%.o}.err"; then
        echo "  FAIL: $(basename "$src")"
        cat "${obj%.o}.err"
        return 1
    fi
    rm -f "${obj%.o}.err"
    return 0
}

COMPILE_S() {
    local src="$1"
    local obj="$2"
    if ! $CC $CF -c "$src" -o "$obj" 2>"${obj%.o}.err"; then
        echo "  FAIL: $(basename "$src")"
        cat "${obj%.o}.err"
        return 1
    fi
    rm -f "${obj%.o}.err"
    return 0
}

ERRORS=0

# ── Compile ───────────────────────────────────────────────────────────────────
echo "=== Building FreeRTOS rtos-benchmark for RK3588 Orange Pi 5 Plus ==="
echo "  CC:      $CC"
echo "  Kernel:  $KERNEL"
echo "  Output:  $BD"
echo "  Iter:    $ITERATIONS"
echo "  Calib:   $CALIBRATION_LOOPS"
echo ""

echo "--- Startup & FreeRTOS port ---"
COMPILE_S "${SRC}/startup_aarch64.S"              "${BD}/startup.o"            || ERRORS=$((ERRORS+1))
COMPILE_S "${SRC}/port/portASM.S"                  "${BD}/portASM.o"            || ERRORS=$((ERRORS+1))
COMPILE_C "${SRC}/port/port.c"                     "${BD}/port.o"               || ERRORS=$((ERRORS+1))

echo "--- Board drivers (16550 + GICv3) ---"
COMPILE_C "${SRC}/board/uart_16550.c"              "${BD}/uart_16550.o"         || ERRORS=$((ERRORS+1))
COMPILE_C "${SRC}/board/gicv3.c"                   "${BD}/gicv3.o"              || ERRORS=$((ERRORS+1))
COMPILE_C "${SRC}/board/rk3588_timer.c"            "${BD}/rk3588_timer.o"       || ERRORS=$((ERRORS+1))

echo "--- Architecture & timer ---"
COMPILE_C "${SRC}/arch/arch_util.c"                "${BD}/arch_util.o"          || ERRORS=$((ERRORS+1))
COMPILE_C "${SRC}/timer/bench_generic_timer.c"     "${BD}/bench_generic_timer.o"|| ERRORS=$((ERRORS+1))

echo "--- Benchmark porting layer ---"
COMPILE_C "${SRC}/bench_porting_layer_aarch64.c"   "${BD}/bench_porting.o"      || ERRORS=$((ERRORS+1))
COMPILE_C "${SRC}/bare_metal.c"                    "${BD}/bare_metal.o"         || ERRORS=$((ERRORS+1))

echo "--- FreeRTOS kernel ---"
COMPILE_C "${KERNEL}/tasks.c"                      "${BD}/tasks.o"              || ERRORS=$((ERRORS+1))
COMPILE_C "${KERNEL}/queue.c"                      "${BD}/queue.o"              || ERRORS=$((ERRORS+1))
COMPILE_C "${KERNEL}/list.c"                       "${BD}/list.o"               || ERRORS=$((ERRORS+1))
COMPILE_C "${KERNEL}/timers.c"                     "${BD}/timers.o"             || ERRORS=$((ERRORS+1))
COMPILE_C "${KERNEL}/stream_buffer.c"              "${BD}/stream_buffer.o"      || ERRORS=$((ERRORS+1))
COMPILE_C "${KERNEL}/event_groups.c"               "${BD}/event_groups.o"       || ERRORS=$((ERRORS+1))

echo "--- Benchmark framework ---"
COMPILE_C "${SRC}/bench_utils.c"                   "${BD}/bench_utils.o"        || ERRORS=$((ERRORS+1))
COMPILE_C "${SRC}/bench_all.c"                     "${BD}/bench_all.o"          || ERRORS=$((ERRORS+1))

echo "--- Benchmark tests ---"
for test in thread malloc_free message_queue mutex_lock_unlock \
            sem_context_switch sem_signal_release thread_switch_yield \
            interrupt_latency; do
    if [ -f "${SRC}/tests/bench_${test}_test.c" ]; then
        COMPILE_C "${SRC}/tests/bench_${test}_test.c" "${BD}/bench_${test}_test.o" || ERRORS=$((ERRORS+1))
    fi
done

if [ "$ERRORS" -gt 0 ]; then
    echo ""
    echo "=== COMPILATION FAILED ($ERRORS errors) ==="
    exit 1
fi

# ── Link ──────────────────────────────────────────────────────────────────────
echo ""
echo "--- Linking ---"

if ! $CC -nostdlib -Wl,--no-warn-rwx-segments \
    -T "${PROJ}/rk3588_aarch64.ld" \
    "${BD}/startup.o" \
    "${BD}/portASM.o" \
    "${BD}/port.o" \
    "${BD}/uart_16550.o" \
    "${BD}/gicv3.o" \
    "${BD}/rk3588_timer.o" \
    "${BD}/arch_util.o" \
    "${BD}/bench_generic_timer.o" \
    "${BD}/bench_porting.o" \
    "${BD}/bare_metal.o" \
    "${BD}/tasks.o" \
    "${BD}/queue.o" \
    "${BD}/list.o" \
    "${BD}/timers.o" \
    "${BD}/stream_buffer.o" \
    "${BD}/event_groups.o" \
    "${BD}/bench_utils.o" \
    "${BD}/bench_all.o" \
    "${BD}/bench_thread_test.o" \
    "${BD}/bench_malloc_free_test.o" \
    "${BD}/bench_message_queue_test.o" \
    "${BD}/bench_mutex_lock_unlock_test.o" \
    "${BD}/bench_sem_context_switch_test.o" \
    "${BD}/bench_sem_signal_release_test.o" \
    "${BD}/bench_thread_switch_yield_test.o" \
    "${BD}/bench_interrupt_latency_test.o" \
    -lgcc \
    -o "${BD}/freertos_aarch64_orangepi.elf" \
    -Wl,-Map="${BD}/freertos_aarch64_orangepi.map" 2>&1; then
    echo ""
    echo "=== LINK FAILED ==="
    exit 1
fi

# ── Generate binary ───────────────────────────────────────────────────────────
$OBJCOPY -O binary "${BD}/freertos_aarch64_orangepi.elf" "${BD}/freertos_aarch64_orangepi.bin"

# ── Summary ───────────────────────────────────────────────────────────────────
echo ""
echo "========================================="
echo "  BUILD SUCCESSFUL"
echo "========================================="
echo ""
$SIZE "${BD}/freertos_aarch64_orangepi.elf"
echo ""
BIN_SIZE=$(stat --printf='%s' "${BD}/freertos_aarch64_orangepi.bin")
echo "  ELF:  ${BD}/freertos_aarch64_orangepi.elf"
echo "  BIN:  ${BD}/freertos_aarch64_orangepi.bin (${BIN_SIZE} bytes)"
echo "  MAP:  ${BD}/freertos_aarch64_orangepi.map"
echo ""
echo "Deploy to Orange Pi:"
echo "  scp ${BD}/freertos_aarch64_orangepi.bin root@<orangepi-host>:/tmp/"
echo "  # On Orange Pi u-boot console:"
echo "  => fatload mmc 1:1 0x00200000 freertos_aarch64_orangepi.bin"
echo "  => go 0x00200000"

# ── Optionally flash ──────────────────────────────────────────────────────────
if [ -n "$FLASH" ]; then
    echo ""
    echo "=== Flashing to Orange Pi ($FLASH) ==="
    scp "${BD}/freertos_aarch64_orangepi.bin" "root@${FLASH}:/tmp/freertos_aarch64_orangepi.bin"
    echo "Binary copied to ${FLASH}:/tmp/freertos_aarch64_orangepi.bin"
fi
