add_subdirectory(arch)
add_subdirectory(timer)

target_sources(app PRIVATE board/uart_16550.c)
target_sources(app PRIVATE board/gicv3.c)
target_sources(app PRIVATE board/rk3588_timer.c)
