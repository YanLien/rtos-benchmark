add_subdirectory(arch)
add_subdirectory(timer)

target_sources(app PRIVATE board/pl011_uart.c)
target_sources(app PRIVATE board/gicv3.c)
target_sources(app PRIVATE board/generic_timer.c)
