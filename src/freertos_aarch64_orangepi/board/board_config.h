/* SPDX-License-Identifier: Apache-2.0 */

#ifndef BOARD_CONFIG_H_
#define BOARD_CONFIG_H_

/* UART2 - Orange Pi 5 Plus debug console */
#define UART2_BASE              0xFEB50000UL

#define UART_CONSOLE_BASE       UART2_BASE
#define UART_CONSOLE_BAUD       1500000UL
#define UART_CONSOLE_CLK        24000000UL

/* GIC-600 (GICv3) */
#define GICD_BASE               0xFD000000UL
#define GICR_BASE               0xFD100000UL   /* Base of first Redistributor (CPU0) */
#define GICR_STRIDE             0x20000UL      /* 128 KB per Redistributor */

/* ARM Generic Timer */
#define TIMER_FREQ              24000000UL

#ifndef SYS_CLOCK_HW_CYCLES_PER_SEC
#define SYS_CLOCK_HW_CYCLES_PER_SEC  1800000000UL
#endif

/* EL1 Physical Timer PPI */
#define TIMER_EL1_IRQ           30

/* GIC Distributor register offsets */
#define GICD_CTLR               0x0000
#define GICD_TYPER              0x0004
#define GICD_ISENABLER(n)       (0x0100 + ((n) / 32) * 4)
#define GICD_ICENABLER(n)       (0x0180 + ((n) / 32) * 4)
#define GICD_ICPENDR(n)         (0x0280 + ((n) / 32) * 4)
#define GICD_IPRIORITYR(n)      (0x0400 + (n))
#define GICD_IPRIORITYR_NB(n)   (0x0400 + ((n) & ~3))
#define GICD_IGROUPR(n)         (0x0080 + ((n) / 32) * 4)
#define GICD_IGROUPMODR(n)      (0x0C00 + ((n) / 32) * 4)

/* GIC Redistributor register offsets */
#define GICR_WAKER              0x0014
#define GICR_SGI_BASE           0x10000
#define GICR_IGROUPR0           (GICR_SGI_BASE + 0x0080)
#define GICR_IGROUPMODR0        (GICR_SGI_BASE + 0x0C00)
#define GICR_ISENABLER0         (GICR_SGI_BASE + 0x0100)
#define GICR_ICENABLER0         (GICR_SGI_BASE + 0x0180)
#define GICR_ICPENDR0           (GICR_SGI_BASE + 0x0280)
#define GICR_IPRIORITYR(n)      (GICR_SGI_BASE + 0x0400 + (n))

#endif /* BOARD_CONFIG_H_ */
