/* SPDX-License-Identifier: Apache-2.0 */

#ifndef ORANGE_PI_5_H_
#define ORANGE_PI_5_H_

/*
 * RK3588 hardware address definitions for Orange Pi 5
 * RK3588: 4x Cortex-A76 + 4x Cortex-A55, ARMv8.2-A
 * Targeting Cortex-A55 (LITTLE cluster, CPU0-3)
 */

/* UART2 - Orange Pi 5 debug console */
#define UART2_BASE              0xFEB50000UL

/* GIC-600 (GICv3) */
#define GICD_BASE               0xFD000000UL   /* Distributor */
#define GICR_BASE               0xFD100000UL   /* Redistributor (PPI 0-31) */

/* ARM Generic Timer */
#define TIMER_FREQ              24000000UL     /* 24 MHz counter frequency */

/* CPU clock - Cortex-A55 on RK3588 */
#ifndef SYS_CLOCK_HW_CYCLES_PER_SEC
#define SYS_CLOCK_HW_CYCLES_PER_SEC  1800000000UL
#endif

/* EL1 Physical Timer IRQ (GIC SPI ID = 30 + 32 = 62 in GICv3) */
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

#endif /* ORANGE_PI_5_H_ */
