# SPDX-License-Identifier: Apache-2.0
#
# Build configuration for FreeRTOS on AArch64 (Orange Pi 5 Plus, RK3588, Cortex-A55).
# This fragment is included by the root CMakeLists.txt.

if (NOT FREERTOS_KERNEL_PATH)
    message(FATAL_ERROR "Please inform FreeRTOS-Kernel path via FREERTOS_KERNEL_PATH")
endif()

# Compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DFREERTOS_AARCH64 -DFREERTOS_AARCH64_ORANGEPI")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DSYS_CLOCK_HW_CYCLES_PER_SEC=1800000000")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mcpu=cortex-a55")
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
include_directories(src/freertos_aarch64_orangepi)
include_directories(src/freertos_aarch64_orangepi/board)
include_directories(src/freertos_aarch64_orangepi/port)
include_directories(${FREERTOS_KERNEL_PATH}/include)

# Main benchmark porting layer
add_executable(app src/freertos_aarch64_orangepi/bench_porting_layer_aarch64.c)

# Board drivers
target_sources(app PRIVATE src/freertos_aarch64_orangepi/board/uart_16550.c)
target_sources(app PRIVATE src/freertos_aarch64_orangepi/board/gicv3.c)
target_sources(app PRIVATE src/freertos_aarch64_orangepi/board/rk3588_timer.c)

# Architecture-specific timing
target_sources(app PRIVATE src/freertos_aarch64_orangepi/arch/aarch64/arch_util.c)

# Timer interrupt layer
target_sources(app PRIVATE src/freertos_aarch64_orangepi/timer/bench_generic_timer.c)

# Startup code (includes exception vector table)
target_sources(app PRIVATE src/freertos_aarch64_orangepi/startup_aarch64.S)

# Bare metal support functions (memset, memcpy, main, assert)
target_sources(app PRIVATE src/freertos_aarch64_orangepi/bare_metal.c)

# Custom FreeRTOS AArch64 port (GICv3 system register interface)
target_sources(app PRIVATE src/freertos_aarch64_orangepi/port/port.c)
target_sources(app PRIVATE src/freertos_aarch64_orangepi/port/portASM.S)

# FreeRTOS kernel core sources (no standard port files)
target_sources(app PRIVATE ${FREERTOS_KERNEL_PATH}/tasks.c)
target_sources(app PRIVATE ${FREERTOS_KERNEL_PATH}/queue.c)
target_sources(app PRIVATE ${FREERTOS_KERNEL_PATH}/list.c)
target_sources(app PRIVATE ${FREERTOS_KERNEL_PATH}/timers.c)
target_sources(app PRIVATE ${FREERTOS_KERNEL_PATH}/stream_buffer.c)
target_sources(app PRIVATE ${FREERTOS_KERNEL_PATH}/event_groups.c)

# Linker script
set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/src/freertos_aarch64_orangepi/rk3588_aarch64.ld)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -T${LINKER_SCRIPT}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -nostdlib")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--gc-sections")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-Map=freertos_aarch64_orangepi.map")

target_link_libraries(app PRIVATE -Wl,--start-group gcc -Wl,--end-group)

# Output binary
set(EXEC_NAME freertos_aarch64_orangepi.elf)
set_target_properties(app PROPERTIES OUTPUT_NAME ${EXEC_NAME})

# Generate binary for u-boot loading
add_custom_command(TARGET app POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary ${EXEC_NAME} freertos_aarch64_orangepi.bin
    COMMENT "Generating binary: freertos_aarch64_orangepi.bin"
)

# Flash target - using scp to copy to Orange Pi 5
add_custom_target(flash USES_TERMINAL DEPENDS app
    COMMAND scp freertos_aarch64_orangepi.bin root@orangepi5:/tmp/freertos_aarch64_orangepi.bin
    COMMENT "Copying binary to Orange Pi 5"
)
