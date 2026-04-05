/* SPDX-License-Identifier: Apache-2.0 */

#ifndef QEMU_VIRT_H_
#define QEMU_VIRT_H_

/*
 * QEMU 'virt' machine hardware address definitions for AArch64.
 * qemu-system-aarch64 -M virt,gic-version=3 -cpu cortex-a55
 */

/* PL011 UART0 - QEMU virt console */
#define UART_CONSOLE_BASE      0x09000000UL
#define UART_CONSOLE_BAUD      115200UL
#define UART_CONSOLE_CLK       24000000UL

/* Alias for code that uses UART2_BASE */
#define UART2_BASE             UART_CONSOLE_BASE

/* GICv3 (QEMU virt) */
#define GICD_BASE              0x08000000UL   /* Distributor */
#define GICR_BASE              0x080A0000UL   /* Redistributor (core 0) */
#define GICR_STRIDE            0x20000UL      /* 128 KB per Redistributor pair */

/* ARM Generic Timer - QEMU sets CNTFRQ_EL0 to 62.5 MHz */
#define TIMER_FREQ             62500000UL     /* 62.5 MHz counter frequency */

/* CPU clock - Cortex-A55 in QEMU (matches CNTFRQ_EL0) */
#define SYS_CLOCK_HW_CYCLES_PER_SEC  62500000UL

/* EL1 Physical Timer IRQ (PPI 30 - same on all GICv3 platforms) */
#define TIMER_EL1_IRQ          30

/* GIC Distributor register offsets (GICv3 architecture-defined) */
#define GICD_CTLR               0x0000
#define GICD_TYPER              0x0004
#define GICD_ISENABLER(n)       (0x0100 + ((n) / 32) * 4)
#define GICD_ICENABLER(n)       (0x0180 + ((n) / 32) * 4)
#define GICD_ICPENDR(n)         (0x0280 + ((n) / 32) * 4)
#define GICD_IPRIORITYR(n)      (0x0400 + (n))
#define GICD_IPRIORITYR_NB(n)   (0x0400 + ((n) & ~3))
#define GICD_IGROUPR(n)         (0x0080 + ((n) / 32) * 4)
#define GICD_IGROUPMODR(n)      (0x0C00 + ((n) / 32) * 4)
#define GICD_SGIR               0x0F00

/* GIC Redistributor register offsets */
#define GICR_CTLR               0x0000
#define GICR_TYPER              0x0008
#define GICR_WAKER              0x0014
#define GICR_SGI_BASE           0x10000
#define GICR_IGROUPR0           (GICR_SGI_BASE + 0x0080)
#define GICR_IGROUPMODR0        (GICR_SGI_BASE + 0x0C00)
#define GICR_ISENABLER0         (GICR_SGI_BASE + 0x0100)
#define GICR_ICENABLER0         (GICR_SGI_BASE + 0x0180)
#define GICR_ICPENDR0           (GICR_SGI_BASE + 0x0280)
#define GICR_IPRIORITYR(n)      (GICR_SGI_BASE + 0x0400 + (n))

#endif /* QEMU_VIRT_H_ */
