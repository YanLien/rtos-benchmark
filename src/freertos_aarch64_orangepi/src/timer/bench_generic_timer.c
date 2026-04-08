/* SPDX-License-Identifier: Apache-2.0 */

/*
 * Timer interrupt abstraction for benchmark interrupt latency tests.
 * Uses the EL1 Physical Timer, which is the timer path already proven to
 * reach the Orange Pi EL1 IRQ handler.
 */

#include "bench_api.h"
#include "board_config.h"
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

void bench_exit_timer_isr(void)
{
	write_cntp_ctl_el0(0);
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
	timer_isr_handler = (handler != NULL) ? handler : freertos_tick_isr;

	/* Enable the physical timer IRQ used by both the tick and the benchmark. */
	gicv3_enable_interrupt(TIMER_EL1_IRQ);
	gicv3_set_priority(TIMER_EL1_IRQ, configMAX_API_CALL_INTERRUPT_PRIORITY);
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
	timer_isr_handler = (handler != NULL) ? handler : freertos_tick_isr;

	/* Re-arm the shared physical timer for the normal FreeRTOS tick. */
	write_cntp_tval_el0(bench_timer_cycles_per_tick());
	write_cntp_ctl_el0(0x1);
}
