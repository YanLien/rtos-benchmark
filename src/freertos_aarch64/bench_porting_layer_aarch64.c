/* SPDX-License-Identifier: Apache-2.0 */

#include "bench_api.h"
#include "bench_porting_layer_aarch64.h"

/* FreeRTOS kernel includes */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
#include "semphr.h"

/* Board drivers */
#include "orange_pi_5.h"
#include "uart_16550.h"
#include "gicv3.h"

#include "arch_api.h"

#include <assert.h>

#define MAX_SEMAPHORES 5
#define MAX_THREADS 10
#define STACK_SIZE (configMINIMAL_STACK_SIZE + 200)
#define MAX_MUTEXES 5
#define MAX_QUEUES 1
#define QUEUE_SIZE (1)

static SemaphoreHandle_t semaphores[MAX_SEMAPHORES];
static StaticSemaphore_t semaphore_buffer[MAX_SEMAPHORES];

static TaskHandle_t threads[MAX_THREADS];
static StackType_t stack_buffer[MAX_THREADS][STACK_SIZE];
static StackType_t init_stack_buffer[STACK_SIZE];
static StaticTask_t task_buffer[MAX_THREADS];
static StaticTask_t init_task_buffer;

static SemaphoreHandle_t mutexes[MAX_MUTEXES];
static StaticSemaphore_t mutex_buffers[MAX_MUTEXES];

static TaskHandle_t threads_to_remove[MAX_THREADS];
static int threads_to_remove_idx;
static SemaphoreHandle_t to_remove_sem;
static StaticSemaphore_t to_remove_sem_buf;

static QueueHandle_t queues[MAX_QUEUES];
static uint8_t queue_storage[MAX_QUEUES][QUEUE_SIZE];
static StaticQueue_t queue_buffer[MAX_QUEUES];

#define benchmark_task_PRIORITY (configMAX_PRIORITIES - 1)

/*
 * FreeRTOS tick interrupt setup and handler.
 * These are called by the FreeRTOS AArch64 port.
 */
void vSetupTickInterrupt(void)
{
	/* EL1 Physical Timer is set up in board/rk3588_timer.c */
	extern void rk3588_timer_setup_tick(uint32_t tick_hz);
	rk3588_timer_setup_tick(configTICK_RATE_HZ);
}

void vClearTickInterrupt(void)
{
	extern void rk3588_timer_clear_irq(uint32_t tick_hz);
	rk3588_timer_clear_irq(configTICK_RATE_HZ);
}

/*
 * Application IRQ handler - called by FreeRTOS IRQ handler.
 * We only handle the EL1 Physical Timer IRQ here.
 */
void vApplicationIRQHandler(uint32_t intid)
{
	extern void FreeRTOS_Tick_Handler(void);

	if (intid == TIMER_EL1_IRQ) {
		FreeRTOS_Tick_Handler();
	}
}

void bench_test_init(void (*test_init_function)(void *))
{
	TaskHandle_t handle;

	/* Initialize UART for debug output */
	uart_init(UART2_BASE, 1500000, 24000000);

	PRINTF("FreeRTOS benchmark on RK3588 (Cortex-A55)\r\n");

	/* Initialize GIC */
	gicv3_init();

	/* Initialize Generic Timer */
	extern void rk3588_timer_init(void);
	rk3588_timer_init();

	to_remove_sem = xSemaphoreCreateCountingStatic(1, 0,
						       &to_remove_sem_buf);

	handle = xTaskCreateStatic(test_init_function, "benchmark", STACK_SIZE,
				   NULL, benchmark_task_PRIORITY,
				   init_stack_buffer, &init_task_buffer);
	if (handle == NULL) {
		PRINTF("Task creation failed!\r\n");
		for (;;) {
		}
	}

	PRINTF("Starting FreeRTOS scheduler...\r\n");
	vTaskStartScheduler();

	/* Should never reach here */
	PRINTF("Scheduler exited unexpectedly!\r\n");
	for (;;) {
	}
}

/* ---- Timing functions ---- */

void bench_timing_init(void)
{
	arch_timing_init();
}

void bench_timing_start(void)
{
	arch_timing_start();
}

void bench_timing_stop(void)
{
	arch_timing_stop();
}

bench_time_t bench_timing_counter_get(void)
{
	return arch_timing_counter_get();
}

bench_time_t bench_timing_cycles_get(bench_time_t *time_start, bench_time_t *time_end)
{
	return arch_timing_cycles_get(time_start, time_end);
}

bench_time_t bench_timing_cycles_to_ns(bench_time_t cycles)
{
	return arch_timing_cycles_to_ns(cycles);
}

/* ---- Semaphore functions ---- */

int bench_sem_create(int sem_id, int initial_count, int maximum_count)
{
	SemaphoreHandle_t handle;

	handle = xSemaphoreCreateCountingStatic(maximum_count, initial_count,
						&semaphore_buffer[sem_id]);
	semaphores[sem_id] = handle;

	return BENCH_SUCCESS;
}

void bench_sem_give(int sem_id)
{
	xSemaphoreGive(semaphores[sem_id]);
}

void bench_sem_give_from_isr(int sem_id)
{
	xSemaphoreGiveFromISR(semaphores[sem_id], NULL);
}

int bench_sem_take(int sem_id)
{
	xSemaphoreTake(semaphores[sem_id], portMAX_DELAY);
	return BENCH_SUCCESS;
}

/* ---- Mutex functions ---- */

int bench_mutex_create(int mutex_id)
{
	assert(mutex_id < MAX_MUTEXES);

	mutexes[mutex_id] =
		xSemaphoreCreateRecursiveMutexStatic(&mutex_buffers[mutex_id]);
	return BENCH_SUCCESS;
}

int bench_mutex_lock(int mutex_id)
{
	xSemaphoreTakeRecursive(mutexes[mutex_id], portMAX_DELAY);
	return BENCH_SUCCESS;
}

int bench_mutex_unlock(int mutex_id)
{
	xSemaphoreGiveRecursive(mutexes[mutex_id]);
	return BENCH_SUCCESS;
}

/* ---- Thread functions ---- */

static UBaseType_t map_prio(int prio)
{
	/* FreeRTOS: lower number = lower priority, remap for benchmark */
	assert(prio >= 0);
	return configMAX_PRIORITIES - prio;
}

void bench_thread_set_priority(int priority)
{
	vTaskPrioritySet(NULL, map_prio(priority));
}

int bench_thread_create(int thread_id, const char *thread_name, int priority,
	void (*entry_function)(void *), void *args)
{
	TaskHandle_t handle;

	if (thread_id < 0 || thread_id >= MAX_THREADS)
		return BENCH_ERROR;

	handle = xTaskCreateStatic(entry_function, thread_name, STACK_SIZE,
			   args, map_prio(priority),
			   stack_buffer[thread_id],
			   &task_buffer[thread_id]);

	threads[thread_id] = handle;

	if (handle == NULL)
		return BENCH_ERROR;

	return BENCH_SUCCESS;
}

int bench_thread_spawn(int thread_id, const char *thread_name, int priority,
	void (*entry_function)(void *), void *args)
{
	/* FreeRTOS does not distinguish create from spawn */
	return bench_thread_create(thread_id, thread_name, priority,
				   entry_function, args);
}

void bench_thread_start(int thread_id)
{
	ARG_UNUSED(thread_id);
	/* No-op: FreeRTOS tasks start when created */
}

void bench_thread_resume(int thread_id)
{
	vTaskResume(threads[thread_id]);
}

void bench_thread_suspend(int thread_id)
{
	vTaskSuspend(threads[thread_id]);
}

void bench_thread_abort(int thread_id)
{
	vTaskDelete(threads[thread_id]);
}

void bench_yield(void)
{
	taskYIELD();
}

void bench_sync_ticks(void)
{
}

void bench_thread_exit(void)
{
	threads_to_remove[threads_to_remove_idx++] = xTaskGetCurrentTaskHandle();
	xSemaphoreTake(to_remove_sem, portMAX_DELAY);
}

void bench_collect_resources(void)
{
	while (threads_to_remove_idx) {
		vTaskDelete(threads_to_remove[--threads_to_remove_idx]);
	}
}

/* ---- Memory functions ---- */

void *bench_malloc(size_t size)
{
	(void)size;
	return NULL;   /* Not expected to be used */
}

void bench_free(void *ptr)
{
	(void)ptr;
}

/* ---- Message queue functions ---- */

int bench_message_queue_create(int mq_id, const char *mq_name,
	size_t msg_max_num, size_t msg_max_len)
{
	configASSERT(msg_max_len <= QUEUE_SIZE);

	queues[mq_id] = xQueueCreateStatic(msg_max_num, msg_max_len,
		queue_storage[mq_id], &queue_buffer[mq_id]);

	if (queues[mq_id] == NULL)
		return BENCH_ERROR;

	return BENCH_SUCCESS;
}

int bench_message_queue_send(int mq_id, char *msg_ptr, size_t msg_len)
{
	BaseType_t ret;

	ret = xQueueSend(queues[mq_id], msg_ptr, portMAX_DELAY);

	if (ret != pdPASS)
		return BENCH_ERROR;

	return BENCH_SUCCESS;
}

int bench_message_queue_receive(int mq_id, char *msg_ptr, size_t msg_len)
{
	BaseType_t ret;

	ret = xQueueReceive(queues[mq_id], msg_ptr, portMAX_DELAY);

	if (ret != pdPASS)
		return BENCH_ERROR;

	return BENCH_SUCCESS;
}

int bench_message_queue_delete(int mq_id, const char *mq_name)
{
	vQueueDelete(queues[mq_id]);
	return BENCH_SUCCESS;
}

/* ---- Idle/Timer task memory (required for static allocation) ---- */

static StaticTask_t xIdleTaskTCBBuffer;
static StackType_t xIdleStack[STACK_SIZE];

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
				   StackType_t **ppxIdleTaskStackBuffer,
				   configSTACK_DEPTH_TYPE *pulIdleTaskStackSize)
{
	*ppxIdleTaskTCBBuffer = &xIdleTaskTCBBuffer;
	*ppxIdleTaskStackBuffer = xIdleStack;
	*pulIdleTaskStackSize = STACK_SIZE;
}

static StaticTask_t xTimerTaskTCBBuffer;
static StackType_t xTimerStack[STACK_SIZE];

void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer,
				    StackType_t **ppxTimerTaskStackBuffer,
				    configSTACK_DEPTH_TYPE *pulTimerTaskStackSize)
{
	*ppxTimerTaskTCBBuffer = &xTimerTaskTCBBuffer;
	*ppxTimerTaskStackBuffer = xTimerStack;
	*pulTimerTaskStackSize = STACK_SIZE;
}
