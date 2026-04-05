# SPDX-License-Identifier: Apache-2.0
#
# Build configuration for FreeRTOS on AArch64 (QEMU virt, Cortex-A55).
# This fragment is included by the root CMakeLists.txt.

if (NOT FREERTOS_KERNEL_PATH)
    message(FATAL_ERROR "Please inform FreeRTOS-Kernel path via FREERTOS_KERNEL_PATH")
endif()

# QEMU is used primarily for bring-up/validation, so use smaller defaults unless
# the caller explicitly overrides the top-level cache values.
set(QEMU_ITERATIONS ${ITERATIONS})
if (QEMU_ITERATIONS STREQUAL "10000")
    set(QEMU_ITERATIONS 100)
endif()

set(QEMU_CALIBRATION_LOOPS ${CALIBRATION_LOOPS})
if (QEMU_CALIBRATION_LOOPS STREQUAL "10000")
    set(QEMU_CALIBRATION_LOOPS 1000)
endif()

# Compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DFREERTOS_AARCH64 -DFREERTOS_AARCH64_QEMU")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DSYS_CLOCK_HW_CYCLES_PER_SEC=62500000")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -UITERATIONS -DITERATIONS=${QEMU_ITERATIONS}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -UCALIBRATION_LOOPS -DCALIBRATION_LOOPS=${QEMU_CALIBRATION_LOOPS}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mcpu=cortex-a55")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mgeneral-regs-only")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffreestanding")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -nostdlib")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wextra")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-parameter -Wno-unused-variable")

# Allow the toolchain to be specified (default: aarch64-none-elf-gcc)
if (NOT CMAKE_C_COMPILER)
    set(CMAKE_C_COMPILER aarch64-none-elf-gcc)
endif()

enable_language(ASM)

# Include directories
include_directories(src/freertos_aarch64_qemu)
include_directories(src/freertos_aarch64_qemu/board)
include_directories(src/freertos_aarch64_qemu/port)
include_directories(${FREERTOS_KERNEL_PATH}/include)

# Main benchmark porting layer
add_executable(app src/freertos_aarch64_qemu/bench_porting_layer_aarch64.c)

# Board drivers (PL011 instead of 16550)
target_sources(app PRIVATE src/freertos_aarch64_qemu/board/pl011_uart.c)
target_sources(app PRIVATE src/freertos_aarch64_qemu/board/gicv3.c)
target_sources(app PRIVATE src/freertos_aarch64_qemu/board/generic_timer.c)

# Architecture-specific timing
target_sources(app PRIVATE src/freertos_aarch64_qemu/arch/aarch64/arch_util.c)

# Timer interrupt layer
target_sources(app PRIVATE src/freertos_aarch64_qemu/timer/bench_generic_timer.c)

# Startup code (includes exception vector table)
target_sources(app PRIVATE src/freertos_aarch64_qemu/startup_aarch64.S)

# Bare metal support functions (memset, memcpy, main, assert)
target_sources(app PRIVATE src/freertos_aarch64_qemu/bare_metal.c)

# Custom FreeRTOS AArch64 port (GICv3 system register interface)
target_sources(app PRIVATE src/freertos_aarch64_qemu/port/port.c)
target_sources(app PRIVATE src/freertos_aarch64_qemu/port/portASM.S)

# FreeRTOS kernel core sources (no standard port files)
target_sources(app PRIVATE ${FREERTOS_KERNEL_PATH}/tasks.c)
target_sources(app PRIVATE ${FREERTOS_KERNEL_PATH}/queue.c)
target_sources(app PRIVATE ${FREERTOS_KERNEL_PATH}/list.c)
target_sources(app PRIVATE ${FREERTOS_KERNEL_PATH}/timers.c)
target_sources(app PRIVATE ${FREERTOS_KERNEL_PATH}/stream_buffer.c)
target_sources(app PRIVATE ${FREERTOS_KERNEL_PATH}/event_groups.c)

# Linker script
set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/src/freertos_aarch64_qemu/qemu_virt_aarch64.ld)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -T${LINKER_SCRIPT}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -nostdlib")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--gc-sections")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-Map=freertos_aarch64_qemu.map")

target_link_libraries(app PRIVATE -Wl,--start-group gcc -Wl,--end-group)

# Output binary
set(EXEC_NAME freertos_aarch64_qemu.elf)
set_target_properties(app PROPERTIES OUTPUT_NAME ${EXEC_NAME})

# Generate binary for QEMU loading
add_custom_command(TARGET app POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary ${EXEC_NAME} freertos_aarch64_qemu.bin
    COMMENT "Generating binary: freertos_aarch64_qemu.bin"
)

# QEMU run target
add_custom_target(qemu USES_TERMINAL DEPENDS app
    COMMAND qemu-system-aarch64
        -M virt,gic-version=3
        -cpu cortex-a55
        -smp 4
        -m 256
        -nographic
        -kernel ${EXEC_NAME}
    COMMENT "Running benchmark in QEMU (Cortex-A55)"
)
