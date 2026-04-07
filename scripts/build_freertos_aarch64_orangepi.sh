#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Build FreeRTOS rtos-benchmark for RK3588 Orange Pi 5 Plus (Cortex-A55)
#
# Usage:
#   bash scripts/build_freertos_aarch64_orangepi.sh [OPTIONS]
#
# Options:
#   -k, --kernel PATH    FreeRTOS-Kernel source path
#   -o, --output PATH    Build output directory
#   -f, --flash HOST     Flash binary to Orange Pi via scp
#   -c, --clean          Clean before build
#   -h, --help           Show this help
#
# Environment variables:
#   CROSS_COMPILE        Cross compiler prefix (default: aarch64-linux-gnu-)
#   FREERTOS_KERNEL_PATH FreeRTOS-Kernel source path (overridden by -k)
#   ITERATIONS           Benchmark iterations (default: 10000)
#   CALIBRATION_LOOPS    Calibration loops (default: 10000)

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJ="$(cd "${SCRIPT_DIR}/.." && pwd)"

# Default to project-root FreeRTOS-Kernel if not specified
KERNEL_FLAG=""
if [ -z "${FREERTOS_KERNEL_PATH}" ] && ! echo "$@" | grep -q '\-k\|--kernel'; then
    KERNEL_FLAG="-k ${PROJ}/FreeRTOS-Kernel"
fi

exec bash "${PROJ}/src/freertos_aarch64_orangepi/build.sh" $KERNEL_FLAG "$@"
