/* SPDX-License-Identifier: Apache-2.0 */

#include "gicv3.h"
#include "qemu_virt.h"

#include <stdint.h>
#include <stddef.h>

static inline void write32(uintptr_t addr, uint32_t val)
{
	*(volatile uint32_t *)addr = val;
}

static inline uint32_t read32(uintptr_t addr)
{
	return *(volatile uint32_t *)addr;
}

/* GICv3 system register access via inline assembly */

static inline void write_icc_pmr_el1(uint64_t val)
{
	__asm__ volatile ("msr ICC_PMR_EL1, %0" :: "r" (val));
}

static inline uint64_t read_icc_iar0_el1(void)
{
	uint64_t val;
	__asm__ volatile ("mrs %0, ICC_IAR0_EL1" : "=r" (val));
	return val;
}

static inline uint64_t read_icc_iar1_el1(void)
{
	uint64_t val;
	__asm__ volatile ("mrs %0, ICC_IAR1_EL1" : "=r" (val));
	return val;
}

static inline void write_icc_eoir0_el1(uint64_t val)
{
	__asm__ volatile ("msr ICC_EOIR0_EL1, %0" :: "r" (val));
}

static inline void write_icc_eoir1_el1(uint64_t val)
{
	__asm__ volatile ("msr ICC_EOIR1_EL1, %0" :: "r" (val));
}

static inline void write_icc_sre_el1(uint64_t val)
{
	__asm__ volatile ("msr ICC_SRE_EL1, %0" :: "r" (val));
}

static inline void write_icc_ctlr_el1(uint64_t val)
{
	__asm__ volatile ("msr ICC_CTLR_EL1, %0" :: "r" (val));
}

static inline void write_icc_igrpen0_el1(uint64_t val)
{
	__asm__ volatile ("msr ICC_IGRPEN0_EL1, %0" :: "r" (val));
}

static inline void write_icc_igrpen1_el1(uint64_t val)
{
	__asm__ volatile ("msr ICC_IGRPEN1_EL1, %0" :: "r" (val));
}

static inline void write_icc_bpr0_el1(uint64_t val)
{
	__asm__ volatile ("msr ICC_BPR0_EL1, %0" :: "r" (val));
}

static inline void dsb(void)
{
	__asm__ volatile ("dsb sy" ::: "memory");
}

static inline void isb(void)
{
	__asm__ volatile ("isb" ::: "memory");
}

/*
 * Wait for Redistributor to report all register writes are complete
 * by polling WAKER.ProcessorSleep == 0 && WAKER.ChildrenAsleep == 0
 */
static void gicr_wait_for_rwp(uintptr_t gicr_base)
{
	uint32_t val;

	/* First, ensure ProcessorSleep is cleared */
	val = read32(gicr_base + GICR_WAKER);
	val &= ~(1U << 1); /* clear ProcessorSleep bit */
	write32(gicr_base + GICR_WAKER, val);

	/* Wait for ChildrenAsleep to clear */
	do {
		val = read32(gicr_base + GICR_WAKER);
	} while (val & (1U << 2)); /* ChildrenAsleep */
}

void gicv3_init(void)
{
	uint32_t i;
	uintptr_t gicd = GICD_BASE;
	uintptr_t gicr = GICR_BASE;

	/* Enable system register interface (SRE) */
	write_icc_sre_el1(0x7);
	isb();

	/* Disable Distributor before configuration */
	write32(gicd + GICD_CTLR, 0x0);
	dsb();

	/* Set all SPI interrupts to Group 1 (IRQ) */
	for (i = 32; i < 1020; i += 32) {
		write32(gicd + GICD_IGROUPR(i), 0xFFFFFFFF);
		write32(gicd + GICD_IGROUPMODR(i), 0x0);
	}

	/* Set all PPI (0-31) to Group 1 */
	write32(gicr + GICR_IGROUPR0, 0xFFFFFFFF);
	write32(gicr + GICR_IGROUPMODR0, 0x0);

	/* Set default priority for all interrupts */
	for (i = 0; i < 1020; i += 4) {
		write32(gicd + GICD_IPRIORITYR_NB(i), 0xA0A0A0A0);
	}
	/* PPI priorities */
	for (i = 0; i < 32; i += 4) {
		write32(gicr + GICR_IPRIORITYR(i), 0xA0A0A0A0);
	}

	/* Disable all SPIs */
	for (i = 32; i < 1020; i += 32) {
		write32(gicd + GICD_ICENABLER(i), 0xFFFFFFFF);
	}
	/* Disable all PPIs/SGIs */
	write32(gicr + GICR_ICENABLER0, 0xFFFFFFFF);

	/* Clear all pending */
	for (i = 32; i < 1020; i += 32) {
		write32(gicd + GICD_ICPENDR(i), 0xFFFFFFFF);
	}

	/* Wait for Redistributor */
	gicr_wait_for_rwp(gicr);

	/* Enable Distributor (Group 1) */
	write32(gicd + GICD_CTLR, 0x2);
	dsb();

	/* Enable SGI 0 for inter-core IPI (yield) at lowest usable priority */
	*(volatile uint8_t *)(gicr + GICR_IPRIORITYR(0)) = 0xFE; /* lowest priority */
	write32(gicr + GICR_ISENABLER0, 1U << 0); /* enable SGI 0 */

	/* Set PMR to allow all priorities */
	write_icc_pmr_el1(0xFF);

	/* Set binary point register */
	write_icc_bpr0_el1(0x0);

	/* Enable Group 1 interrupts at CPU interface */
	write_icc_igrpen1_el1(0x1);
	isb();
}

/*
 * Per-core GICv3 initialization for secondary cores.
 * The Distributor is already configured by the primary core.
 * Each secondary core only needs to configure its own Redistributor
 * and enable its CPU interface.
 */
void gicv3_init_secondary(void)
{
	uint64_t mpidr;
	uint8_t core_id;
	uintptr_t gicr;

	__asm__ volatile ("mrs %0, MPIDR_EL1" : "=r" (mpidr));
	core_id = (uint8_t)(mpidr & 0xff);

	/* Calculate this core's Redistributor base address */
	gicr = GICR_BASE + ((uintptr_t)core_id * GICR_STRIDE);

	/* Enable system register interface */
	write_icc_sre_el1(0x7);
	isb();

	/* Set all PPI (0-31) to Group 1 */
	write32(gicr + GICR_IGROUPR0, 0xFFFFFFFF);
	write32(gicr + GICR_IGROUPMODR0, 0x0);

	/* PPI priorities */
	uint32_t i;
	for (i = 0; i < 32; i += 4) {
		write32(gicr + GICR_IPRIORITYR(i), 0xA0A0A0A0);
	}

	/* Disable all PPIs/SGIs */
	write32(gicr + GICR_ICENABLER0, 0xFFFFFFFF);

	/* Wait for Redistributor */
	gicr_wait_for_rwp(gicr);

	/* Enable SGI 0 for inter-core IPI */
	*(volatile uint8_t *)(gicr + GICR_IPRIORITYR(0)) = 0xFE;
	write32(gicr + GICR_ISENABLER0, 1U << 0);

	/* Set PMR to allow all priorities */
	write_icc_pmr_el1(0xFF);

	/* Set binary point register */
	write_icc_bpr0_el1(0x0);

	/* Enable Group 1 interrupts at CPU interface */
	write_icc_igrpen1_el1(0x1);
	isb();
}

void gicv3_enable_interrupt(uint32_t intid)
{
	uintptr_t base;
	uint32_t reg;

	if (intid < 32) {
		/* PPI/SGI - use Redistributor */
		base = GICR_BASE;
		reg = read32(base + GICR_ISENABLER0);
		reg |= (1U << (intid % 32));
		write32(base + GICR_ISENABLER0, reg);
	} else {
		/* SPI - use Distributor */
		base = GICD_BASE;
		write32(base + GICD_ISENABLER(intid), 1U << (intid % 32));
	}
	dsb();
}

void gicv3_disable_interrupt(uint32_t intid)
{
	uintptr_t base;

	if (intid < 32) {
		base = GICR_BASE;
		write32(base + GICR_ICENABLER0, 1U << (intid % 32));
	} else {
		base = GICD_BASE;
		write32(base + GICD_ICENABLER(intid), 1U << (intid % 32));
	}
	dsb();
}

void gicv3_set_priority(uint32_t intid, uint8_t priority)
{
	if (intid < 32) {
		/* PPI/SGI */
		*(volatile uint8_t *)(GICR_BASE + GICR_IPRIORITYR(intid)) = priority;
	} else {
		/* SPI */
		*(volatile uint8_t *)(GICD_BASE + GICD_IPRIORITYR(intid)) = priority;
	}
	dsb();
}

uint32_t gicv3_acknowledge_irq(void)
{
	return (uint32_t)read_icc_iar1_el1();
}

void gicv3_eoi(uint32_t intid)
{
	write_icc_eoir1_el1((uint64_t)intid);
	isb();
}
