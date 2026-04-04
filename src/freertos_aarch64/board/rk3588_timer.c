/* SPDX-License-Identifier: Apache-2.0 */

#include "board_config.h"
#include "gicv3.h"

/*
 * ARM Generic Timer driver for RK3588.
 * Uses EL1 Physical Timer (CNTP_TVAL_EL0 / CNTP_CTL_EL0) for tick generation.
 * The counter frequency is typically 24 MHz (set by firmware via CNTFRQ_EL0).
 */

static inline uint64_t read_cntpct_el0(void)
{
	uint64_t val;
	__asm__ volatile ("mrs %0, cntpct_el0" : "=r" (val));
	return val;
}

static inline void write_cntp_tval_el0(uint64_t val)
{
	__asm__ volatile ("msr cntp_tval_el0, %0" :: "r" (val));
}

static inline void write_cntp_ctl_el0(uint64_t val)
{
	__asm__ volatile ("msr cntp_ctl_el0, %0" :: "r" (val));
}

static inline uint64_t read_cntp_ctl_el0(void)
{
	uint64_t val;
	__asm__ volatile ("mrs %0, cntp_ctl_el0" : "=r" (val));
	return val;
}

static inline void write_cntfrq_el0(uint64_t val)
{
	__asm__ volatile ("msr cntfrq_el0, %0" :: "r" (val));
}

static inline uint64_t read_cntfrq_el0(void)
{
	uint64_t val;
	__asm__ volatile ("mrs %0, cntfrq_el0" : "=r" (val));
	return val;
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
 * Get the counter frequency. This is typically set by firmware (u-boot/ATF)
 * but we read it from the system register as a fallback.
 */
uint64_t rk3588_timer_get_freq(void)
{
	return read_cntfrq_el0();
}

/*
 * Initialize the EL1 Physical Timer for periodic tick interrupts.
 * The timer is configured but not enabled here - that happens in vPortSetupTimerInterrupt().
 */
void rk3588_timer_init(void)
{
	/* Disable the timer first */
	write_cntp_ctl_el0(0);
	isb();
}

/*
 * Set up a one-shot or periodic tick using EL1 Physical Timer.
 * Called by FreeRTOS to set up the tick interrupt.
 */
void rk3588_timer_setup_tick(uint32_t tick_hz)
{
	uint64_t freq = rk3588_timer_get_freq();
	uint64_t tval = freq / tick_hz;

	/* Disable timer */
	write_cntp_ctl_el0(0);
	isb();

	/* Set the timer value (counts down from tval to 0, then fires IRQ) */
	write_cntp_tval_el0(tval);
	isb();

	/* Enable timer with IRQ generation: bit0=Enable, bit1=IMASK (0=unmasked) */
	write_cntp_ctl_el0(0x1);
	isb();

	/* Enable the timer IRQ in GIC */
	gicv3_enable_interrupt(TIMER_EL1_IRQ);
	gicv3_set_priority(TIMER_EL1_IRQ, 0x80); /* medium priority */
}

/*
 * Clear the EL1 Physical Timer interrupt.
 * Called from the timer ISR to acknowledge and re-arm.
 */
void rk3588_timer_clear_irq(uint32_t tick_hz)
{
	uint64_t freq = rk3588_timer_get_freq();
	uint64_t tval = freq / tick_hz;

	/* Re-arm the timer by writing a new TVAL (auto-clears the IRQ status) */
	write_cntp_tval_el0(tval);
	isb();
}

/*
 * Read the current physical counter value.
 */
uint64_t rk3588_timer_get_counter(void)
{
	return read_cntpct_el0();
}
