/* SPDX-License-Identifier: Apache-2.0 */

#ifndef PORTING_LAYER_AARCH64_H_
#define PORTING_LAYER_AARCH64_H_

#include "FreeRTOS.h"

typedef uint64_t bench_time_t;
typedef void * bench_work;

#include "pl011_uart.h"

#define PRINTF(fmt, ...) uart_printf(fmt, ##__VA_ARGS__)

#define BENCH_LAST_PRIORITY (configMAX_PRIORITIES - 1)
#define BENCH_IDLE_TIME     5

#define __weak __attribute__((__weak__))

#define ARG_UNUSED(x) (void)(x)

/*
 * FreeRTOS on AArch64 supports thread spawn (create + immediate start).
 */
#define RTOS_HAS_THREAD_SPAWN         1
#define RTOS_HAS_THREAD_CREATE_START  0
#define RTOS_HAS_SUSPEND_RESUME       1
#define RTOS_HAS_MAIN_ENTRY_POINT     1

#endif /* PORTING_LAYER_AARCH64_H_ */
