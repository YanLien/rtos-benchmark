/* SPDX-License-Identifier: Apache-2.0 */

#include "gicv3.h"
#include "board_config.h"

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

static inline uint64_t read64(uintptr_t addr)
{
	uint64_t lo = read32(addr);
	uint64_t hi = read32(addr + 4U);

	return lo | (hi << 32);
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

static void gicr_wait_for_write_pending(uintptr_t gicr_base)
{
	while ((read32(gicr_base + GICR_CTLR) & (1U << 3)) != 0U) {
	}
}

static inline uint64_t read_mpidr_el1(void)
{
	uint64_t val;

	__asm__ volatile ("mrs %0, mpidr_el1" : "=r" (val));
	return val;
}

static uintptr_t gicr_base_for_current_cpu(void)
{
	static uintptr_t cached_gicr_base;
	uintptr_t gicr;
	uint64_t mpidr;
	uint64_t affinity;
	uint64_t typer;
	uint32_t i;

	if (cached_gicr_base != 0U)
		return cached_gicr_base;

	mpidr = read_mpidr_el1();
	affinity = ((mpidr >> 8) & 0xFF000000ULL) | (mpidr & 0x00FFFFFFULL);
	gicr = GICR_BASE;

	for (i = 0; i < GICR_FRAME_COUNT; i++) {
		typer = read64(gicr + GICR_TYPER);

		if ((typer >> 32) == affinity) {
			cached_gicr_base = gicr;
			return cached_gicr_base;
		}

		gicr += GICR_FRAME_STRIDE;
	}

	cached_gicr_base = GICR_BASE;
	return cached_gicr_base;
}

typedef void (*gicr_apply_fn_t)(uintptr_t gicr_base, uint32_t intid, uint8_t priority);

static void gicr_apply_to_all_redistributors(gicr_apply_fn_t fn, uint32_t intid, uint8_t priority)
{
	uintptr_t gicr = GICR_BASE;
	uint32_t i;

	for (i = 0; i < GICR_FRAME_COUNT; i++) {
		fn(gicr, intid, priority);
		gicr += GICR_FRAME_STRIDE;
	}
}

/* Wake up the Redistributor: clear ProcessorSleep and wait for ChildrenAsleep to clear */
static void gicr_wakeup(uintptr_t gicr_base)
{
    uint32_t val;

    val = read32(gicr_base + GICR_WAKER);
    val &= ~(1U << 1); /* clear ProcessorSleep */
    write32(gicr_base + GICR_WAKER, val);

    do {
        val = read32(gicr_base + GICR_WAKER);
    } while (val & (1U << 2)); /* wait for ChildrenAsleep == 0 */
}

static void gicr_configure_ppi_defaults(uintptr_t gicr_base, uint32_t intid, uint8_t priority)
{
	uint32_t i;

	(void)intid;
	(void)priority;

	write32(gicr_base + GICR_IGROUPR0, 0xFFFFFFFF);
	write32(gicr_base + GICR_IGROUPMODR0, 0x0);

	for (i = 0; i < 32; i += 4) {
		write32(gicr_base + GICR_IPRIORITYR(i), 0xA0A0A0A0);
	}

	write32(gicr_base + GICR_ICENABLER0, 0xFFFFFFFF);
	gicr_wait_for_write_pending(gicr_base);
	write32(gicr_base + GICR_ICPENDR0, 0xFFFFFFFF);
	gicr_wait_for_write_pending(gicr_base);
}

static void gicr_enable_ppi(uintptr_t gicr_base, uint32_t intid, uint8_t priority)
{
	(void)priority;

	write32(gicr_base + GICR_ISENABLER0, 1U << (intid % 32));
	gicr_wait_for_write_pending(gicr_base);
}

static void gicr_disable_ppi(uintptr_t gicr_base, uint32_t intid, uint8_t priority)
{
	(void)priority;

	write32(gicr_base + GICR_ICENABLER0, 1U << (intid % 32));
	gicr_wait_for_write_pending(gicr_base);
}

static void gicr_set_ppi_priority(uintptr_t gicr_base, uint32_t intid, uint8_t priority)
{
	uintptr_t addr = gicr_base + GICR_IPRIORITYR(intid & ~3U);
	uint32_t shift = (intid & 3U) * 8U;
	uint32_t val = read32(addr);

	val &= ~(0xFFU << shift);
	val |= ((uint32_t)priority) << shift;
	write32(addr, val);
	dsb();
}

void gicv3_init(void)
{
    uint32_t i;
    uintptr_t gicd = GICD_BASE;
    uintptr_t gicr = gicr_base_for_current_cpu();

    write_icc_sre_el1(0x7);
    isb();

    /* Disable Distributor */
    write32(gicd + GICD_CTLR, 0x0);
    dsb();

    /* Group 1 for all SPIs */
    for (i = 32; i < 1020; i += 32) {
        write32(gicd + GICD_IGROUPR(i),    0xFFFFFFFF);
        write32(gicd + GICD_IGROUPMODR(i), 0x0);
    }

    /* Default priority */
    for (i = 0; i < 1020; i += 4) {
        write32(gicd + GICD_IPRIORITYR_NB(i), 0xA0A0A0A0);
    }

    /* Disable and clear pending for all SPIs */
    for (i = 32; i < 1020; i += 32) {
        write32(gicd + GICD_ICENABLER(i), 0xFFFFFFFF);
        write32(gicd + GICD_ICPENDR(i),   0xFFFFFFFF);
    }

    /* Wake up Redistributor, then configure PPIs */
    gicr_wakeup(gicr);
    gicr_configure_ppi_defaults(gicr, 0U, 0U);

    /* Enable Distributor: ARE_NS + EnableGrp1NS */
    write32(gicd + GICD_CTLR, (1U << 4) | (1U << 1));
    dsb();

    write_icc_pmr_el1(0xFF);
    write_icc_bpr0_el1(0x0);
    write_icc_igrpen1_el1(0x1);
    isb();
}

void gicv3_enable_interrupt(uint32_t intid)
{
    if (intid < 32) {
        gicr_apply_to_all_redistributors(gicr_enable_ppi, intid, 0U);
    } else {
        /* Route SPI to CPU0 (mpidr affinity = 0) */
        write32(GICD_BASE + GICD_IROUTER(intid),     0x0U); /* low 32 bits */
        write32(GICD_BASE + GICD_IROUTER(intid) + 4, 0x0U); /* high 32 bits */
        write32(GICD_BASE + GICD_ISENABLER(intid),
                1U << (intid % 32));
    }
    dsb();
}

void gicv3_disable_interrupt(uint32_t intid)
{
	uintptr_t base;

	if (intid < 32) {
		gicr_apply_to_all_redistributors(gicr_disable_ppi, intid, 0U);
	} else {
		base = GICD_BASE;
		write32(base + GICD_ICENABLER(intid), 1U << (intid % 32));
	}
	dsb();
}

void gicv3_set_priority(uint32_t intid, uint8_t priority)
{
	uintptr_t addr;
	uint32_t shift;
	uint32_t val;

	if (intid < 32) {
		/* PPI/SGI */
		gicr_apply_to_all_redistributors(gicr_set_ppi_priority, intid, priority);
	} else {
		/* SPI */
		addr = GICD_BASE + GICD_IPRIORITYR_NB(intid);
		shift = (intid & 3U) * 8U;
		val = read32(addr);
		val &= ~(0xFFU << shift);
		val |= ((uint32_t)priority) << shift;
		write32(addr, val);
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
