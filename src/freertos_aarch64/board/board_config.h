/* SPDX-License-Identifier: Apache-2.0 */

#ifndef BOARD_CONFIG_H_
#define BOARD_CONFIG_H_

/*
 * Board configuration dispatcher.
 * Include the correct board header based on the BOARD_* define.
 *
 * Each board header must provide:
 *   UART2_BASE / UART_CONSOLE_BASE  - Console UART base address
 *   UART_CONSOLE_BAUD               - Console UART baud rate
 *   UART_CONSOLE_CLK                - Console UART input clock frequency
 *   GICD_BASE                       - GICv3 Distributor base address
 *   GICR_BASE                       - GICv3 Redistributor base address
 *   TIMER_FREQ                      - ARM Generic Timer counter frequency (Hz)
 *   TIMER_EL1_IRQ                   - EL1 Physical Timer IRQ number
 *   SYS_CLOCK_HW_CYCLES_PER_SEC     - CPU core clock frequency (Hz)
 *   GIC register offset macros       - GICD_CTLR, GICR_*, etc.
 */

#if defined(BOARD_ORANGE_PI_5)
    #include "orange_pi_5.h"
#elif defined(BOARD_QEMU_VIRT)
    #include "qemu_virt.h"
#else
    #error "No board selected. Define BOARD_ORANGE_PI_5 or BOARD_QEMU_VIRT."
#endif

#endif /* BOARD_CONFIG_H_ */
