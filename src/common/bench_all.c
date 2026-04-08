// SPDX-License-Identifier: Apache-2.0

#include "bench_api.h"
#include "bench_utils.h"

#if defined(BOARD_ORANGE_PI_5)
#include "board_config.h"
#endif

extern void bench_basic_thread_ops(void *arg);
extern void bench_interrupt_latency_test(void *arg);
extern void bench_mutex_lock_unlock_test(void *arg);
extern void bench_sem_context_switch_init(void *arg);
extern void bench_sem_signal_release_init(void *arg);
extern void bench_thread_yield(void *arg);
extern void bench_malloc_free(void *arg);
extern void bench_message_queue_init(void *arg);

#if defined(BOARD_ORANGE_PI_5)
static void bench_raw_uart_stamp(const char *str)
{
	volatile unsigned char *thr = (volatile unsigned char *)(UART2_BASE + 0x00U);
	volatile unsigned char *lsr = (volatile unsigned char *)(UART2_BASE + 0x14U);

	while (*str) {
		while ( (*lsr & (1U << 5)) == 0U ) {
		}
		*thr = (unsigned char)*str++;
	}
}
#endif

void bench_all(void *arg)
{
	/* Raw UART output to test if task started (bypasses PRINTF) */
#ifdef BOARD_QEMU_VIRT
	volatile uint32_t *uart_dr = (volatile uint32_t *)0x09000000;
	*uart_dr = '!'; *uart_dr = 'T'; *uart_dr = 'A'; *uart_dr = 'S'; *uart_dr = 'K';
	*uart_dr = '!'; *uart_dr = '\r'; *uart_dr = '\n';
#endif
#ifdef BOARD_ORANGE_PI_5
	bench_raw_uart_stamp("[TASK-ENTRY]");
#endif

	PRINTF("\n\r *** Starting! ***\n\n\r");
#ifdef BOARD_ORANGE_PI_5
	bench_raw_uart_stamp("[POST-PRINTF]");
#endif

	bench_basic_thread_ops(arg);
	bench_mutex_lock_unlock_test(arg);
	bench_sem_context_switch_init(arg);
	bench_sem_signal_release_init(arg);
	bench_thread_yield(arg);
	bench_malloc_free(arg);
	bench_message_queue_init(arg);

	/* This should be the last test as it can muck with the timer */

	bench_interrupt_latency_test(arg);

	PRINTF("\n\r *** Done! ***\n\r");
}

#if RTOS_HAS_MAIN_ENTRY_POINT
int main(void)
{
	bench_test_init(bench_all);
	return 0;
}
#endif
