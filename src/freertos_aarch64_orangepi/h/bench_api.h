/* SPDX-License-Identifier: Apache-2.0 */

#ifndef  BENCH_API_H
#define  BENCH_API_H

#define BENCH_SUCCESS 0 /* Value returned when operation succeeds */
#define BENCH_ERROR 1 /* Value returned when operation fails */

#ifdef FREERTOS_AARCH64
#include "bench_porting_layer_aarch64.h"
#endif /* FREERTOS_AARCH64 */

typedef void (*bench_isr_handler_t)(void *arg);

void bench_test_init(void (*test_init_function)(void *));
void bench_thread_set_priority(int priority);
int bench_thread_create(int thread_id, const char *thread_name, int priority,
	void (*entry_function)(void *), void *args);
int bench_thread_spawn(int thread_id, const char *thread_name, int priority,
	void (*entry_function)(void *), void *args);
void bench_thread_start(int thread_id);
void bench_thread_resume(int thread_id);
void bench_thread_suspend(int thread_id);
void bench_thread_abort(int thread_id);
void bench_thread_exit(void);
void bench_yield(void);
void bench_timing_init(void);
void bench_sync_ticks(void);
void bench_timing_start(void);
void bench_timing_stop(void);
bench_time_t bench_timing_counter_get(void);
bench_time_t bench_timing_cycles_get(bench_time_t *time_start, bench_time_t *time_end);
bench_time_t bench_timing_cycles_to_ns(bench_time_t cycles);
int bench_sem_create(int sem_id, int initial_count, int maximum_count);
void bench_sem_give(int sem_id);
void bench_sem_give_from_isr(int sem_id);
int bench_sem_take(int sem_id);
int bench_mutex_create(int mutex_id);
int bench_mutex_lock(int mutex_id);
int bench_mutex_unlock(int mutex_id);
void *bench_malloc(size_t size);
void bench_free(void *ptr);
int bench_message_queue_create(int mq_id, const char *mq_name,
	size_t msg_max_num, size_t msg_max_len);
int bench_message_queue_send(int mq_id, char *msg_ptr, size_t msg_len);
int bench_message_queue_receive(int mq_id, char *msg_ptr, size_t msg_len);
int bench_message_queue_delete(int mq_id, const char *mq_name);
bench_isr_handler_t bench_timer_isr_get(void);
void bench_timer_isr_set(bench_isr_handler_t handler);
void bench_timer_isr_restore(bench_isr_handler_t handler);
bench_time_t bench_timer_isr_expiry_set(uint32_t usec);
bench_time_t bench_timer_cycles_diff(bench_time_t trigger, bench_time_t sample);
bench_time_t bench_timer_cycles_get(void);
uint32_t bench_timer_cycles_per_second(void);
uint32_t bench_timer_cycles_per_tick(void);
void bench_collect_resources(void);

#endif /* BENCH_API_H */
