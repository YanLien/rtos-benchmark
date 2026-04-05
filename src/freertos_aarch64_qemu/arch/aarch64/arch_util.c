/* SPDX-License-Identifier: Apache-2.0 */

/*
 * Architecture-specific timing utilities for AArch64 (QEMU virt).
 * Uses ARM Generic Timer physical counter (CNTPCT_EL0).
 */

#include "arch_api.h"
#include "bench_api.h"
#include "qemu_virt.h"

#define NSEC_PER_SEC 1000000000ULL

static inline uint64_t read_cntpct_el0(void)
{
	uint64_t val;
	__asm__ volatile ("mrs %0, cntpct_el0" : "=r" (val));
	return val;
}

/*
 * Note: On AArch64, the cycle counter frequency is the Generic Timer
 * frequency (CNTFRQ_EL0), NOT the CPU core clock frequency.
 * For RK3588, CNTFRQ_EL0 is typically 24 MHz.
 * For cycle-accurate measurement relative to the CPU clock, we would
 * need PMU counters (PMCCNTR_EL0), but the Generic Timer counter is
 * sufficient for benchmarking purposes and is always accessible at EL1.
 */

void arch_timing_init(void)
{
	/* No special initialization needed - CNTPCT_EL0 is always available */
}

void arch_timing_start(void)
{
	/* No action needed - counter runs continuously */
}

void arch_timing_stop(void)
{
	/* No action needed */
}

bench_time_t arch_timing_counter_get(void)
{
	return (bench_time_t)read_cntpct_el0();
}

bench_time_t arch_timing_cycles_get(volatile bench_time_t *const start,
				    volatile bench_time_t *const end)
{
	return (*end - *start);
}

bench_time_t arch_timing_cycles_to_ns(bench_time_t cycles)
{
	/*
	 * Convert Generic Timer counter ticks to nanoseconds.
	 * Use the actual counter frequency from hardware.
	 */
	return (cycles * NSEC_PER_SEC) / TIMER_FREQ;
}
