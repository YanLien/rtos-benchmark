/* SPDX-License-Identifier: Apache-2.0 */

/*
 * Timer interrupt abstraction for benchmark interrupt latency tests.
 * Uses ARM Generic Timer EL1 Physical Timer on RK3588.
 */

#include "bench_api.h"
#include "orange_pi_5.h"
#include "gicv3.h"

extern void FreeRTOS_Tick_Handler(void);

static void freertos_tick_isr(void *arg)
{
	(void)arg;
	FreeRTOS_Tick_Handler();
}

static bench_isr_handler_t timer_isr_handler = freertos_tick_isr;

static inline uint64_t read_cntpct_el0(void)
{
	uint64_t val;
	__asm__ volatile ("mrs %0, cntpct_el0" : "=r" (val));
	return val;
}

static inline void write_cntp_tval_el0(uint64_t val)
{
	__asm__ volatile ("msr cntp_tval_el0, %0" :: "r" (val));
	__asm__ volatile ("isb" ::: "memory");
}

static inline void write_cntp_ctl_el0(uint64_t val)
{
	__asm__ volatile ("msr cntp_ctl_el0, %0" :: "r" (val));
	__asm__ volatile ("isb" ::: "memory");
}

bench_isr_handler_t bench_timer_isr_get(void)
{
	return timer_isr_handler;
}

uint32_t bench_timer_cycles_per_second(void)
{
	return TIMER_FREQ;
}

uint32_t bench_timer_cycles_per_tick(void)
{
	return (bench_timer_cycles_per_second() / configTICK_RATE_HZ);
}

/*
 * Set a new timer ISR handler.
 * On AArch64 with GICv3, we store the handler and enable the timer IRQ.
 */
void bench_timer_isr_set(bench_isr_handler_t handler)
{
	timer_isr_handler = handler;

	/* Enable timer IRQ in GIC */
	gicv3_enable_interrupt(TIMER_EL1_IRQ);
	gicv3_set_priority(TIMER_EL1_IRQ, 0x40); /* high priority for latency test */
}

bench_time_t bench_timer_cycles_diff(bench_time_t trigger_point,
				     bench_time_t sample_point)
{
	return (sample_point - trigger_point);
}

bench_time_t bench_timer_cycles_get(void)
{
	return (bench_time_t)read_cntpct_el0();
}

bench_time_t bench_timer_isr_expiry_set(uint32_t usec)
{
	uint64_t cycles_per_usec = (TIMER_FREQ + 999999) / 1000000;
	uint64_t cycles = cycles_per_usec * usec;
	uint64_t trigger = read_cntpct_el0() + cycles;

	/* Disable timer */
	write_cntp_ctl_el0(0);

	/* Set the timer value */
	write_cntp_tval_el0(cycles);

	/* Enable timer with IRQ unmasked */
	write_cntp_ctl_el0(0x1);

	return (bench_time_t)trigger;
}

void bench_timer_isr_restore(bench_isr_handler_t handler)
{
	/* Restore the original tick handler and re-arm for normal tick */
	timer_isr_handler = (handler != NULL) ? handler : freertos_tick_isr;

	/* Re-arm timer for normal FreeRTOS tick period */
	uint64_t tval = bench_timer_cycles_per_tick();
	write_cntp_tval_el0(tval);
	write_cntp_ctl_el0(0x1);
}
