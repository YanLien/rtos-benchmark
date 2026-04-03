# SPDX-License-Identifier: Apache-2.0
#
# Build configuration for FreeRTOS on AArch64 (RK3588, Orange Pi 5).
# This fragment is included by the root CMakeLists.txt.
#
# Uses a custom GICv3-compatible FreeRTOS port (port/) instead of
# the standard ARM_AARCH64 port which only supports GICv2 MMIO.

if (NOT FREERTOS_KERNEL_PATH)
    message(FATAL_ERROR "Please inform FreeRTOS-Kernel path via FREERTOS_KERNEL_PATH")
endif()

# Compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DFREERTOS_AARCH64")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DSYS_CLOCK_HW_CYCLES_PER_SEC=1800000000")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mcpu=cortex-a76")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mgeneral-regs-only")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffreestanding")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -nostdlib")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wextra")

# Allow the toolchain to be specified (default: aarch64-none-elf-gcc)
if (NOT CMAKE_C_COMPILER)
    set(CMAKE_C_COMPILER aarch64-none-elf-gcc)
endif()

enable_language(ASM)

# Include directories
include_directories(src/freertos_aarch64)
include_directories(src/freertos_aarch64/board)
include_directories(src/freertos_aarch64/port)
include_directories(${FREERTOS_KERNEL_PATH}/include)

# Main benchmark porting layer
add_executable(app src/freertos_aarch64/bench_porting_layer_aarch64.c)

# Board drivers
target_sources(app PRIVATE src/freertos_aarch64/board/uart_16550.c)
target_sources(app PRIVATE src/freertos_aarch64/board/gicv3.c)
target_sources(app PRIVATE src/freertos_aarch64/board/rk3588_timer.c)

# Architecture-specific timing
target_sources(app PRIVATE src/freertos_aarch64/arch/aarch64/arch_util.c)

# Timer interrupt layer
target_sources(app PRIVATE src/freertos_aarch64/timer/bench_generic_timer.c)

# Startup code (includes exception vector table)
target_sources(app PRIVATE src/freertos_aarch64/startup_aarch64.S)

# Bare metal support functions (memset, memcpy, main, assert)
target_sources(app PRIVATE src/freertos_aarch64/bare_metal.c)

# Custom FreeRTOS AArch64 port (GICv3 system register interface)
target_sources(app PRIVATE src/freertos_aarch64/port/port.c)
target_sources(app PRIVATE src/freertos_aarch64/port/portASM.S)

# FreeRTOS kernel core sources (no standard port files)
target_sources(app PRIVATE ${FREERTOS_KERNEL_PATH}/tasks.c)
target_sources(app PRIVATE ${FREERTOS_KERNEL_PATH}/queue.c)
target_sources(app PRIVATE ${FREERTOS_KERNEL_PATH}/list.c)
target_sources(app PRIVATE ${FREERTOS_KERNEL_PATH}/timers.c)
target_sources(app PRIVATE ${FREERTOS_KERNEL_PATH}/stream_buffer.c)
target_sources(app PRIVATE ${FREERTOS_KERNEL_PATH}/event_groups.c)

# Linker script
set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/src/freertos_aarch64/rk3588_aarch64.ld)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -T${LINKER_SCRIPT}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -nostdlib")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--gc-sections")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-Map=freertos_aarch64.map")

target_link_libraries(app PRIVATE -Wl,--start-group gcc -Wl,--end-group)

# Output binary
set(EXEC_NAME freertos_aarch64.elf)
set_target_properties(app PROPERTIES OUTPUT_NAME ${EXEC_NAME})

# Generate binary for u-boot loading
add_custom_command(TARGET app POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary ${EXEC_NAME} freertos_aarch64.bin
    COMMENT "Generating binary: freertos_aarch64.bin"
)

# Flash target - using scp to copy to Orange Pi 5
add_custom_target(flash USES_TERMINAL DEPENDS app
    COMMAND scp freertos_aarch64.bin root@orangepi5:/tmp/freertos_aarch64.bin
    COMMENT "Copying binary to Orange Pi 5"
)
