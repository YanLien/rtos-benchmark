/* SPDX-License-Identifier: Apache-2.0 */

/*
 * FreeRTOS configuration for RK3588 (AArch64, Cortex-A76).
 * Based on FreeRTOS-Kernel portable/GCC/ARM_AARCH64 requirements.
 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include "board_config.h"

/*-----------------------------------------------------------
 * Application specific definitions
 *----------------------------------------------------------*/

#define configUSE_PREEMPTION                    1
#define configUSE_TICKLESS_IDLE                 0
#define configCPU_CLOCK_HZ                      SYS_CLOCK_HW_CYCLES_PER_SEC
#define configTICK_RATE_HZ                      ((TickType_t)1000)
#define configMAX_PRIORITIES                    8
#define configMINIMAL_STACK_SIZE                ((unsigned short)256)
#define configMAX_TASK_NAME_LEN                 20
#define configUSE_16_BIT_TICKS                  0
#define configIDLE_SHOULD_YIELD                 1
#define configUSE_TASK_NOTIFICATIONS            1
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             1
#define configUSE_COUNTING_SEMAPHORES           1
#define configQUEUE_REGISTRY_SIZE               8
#define configUSE_QUEUE_SETS                    0
#define configUSE_TIME_SLICING                  0
#define configUSE_NEWLIB_REENTRANT              0
#define configENABLE_BACKWARD_COMPATIBILITY     0
#define configNUM_THREAD_LOCAL_STORAGE_POINTERS 5

/* Memory allocation - static only */
#define configSUPPORT_STATIC_ALLOCATION         1
#define configSUPPORT_DYNAMIC_ALLOCATION        0
#define configTOTAL_HEAP_SIZE                   ((size_t)(64 * 1024))
#define configAPPLICATION_ALLOCATED_HEAP        0

/* Stack depth type */
#define configSTACK_DEPTH_TYPE                  uint32_t

/* Hook functions */
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configCHECK_FOR_STACK_OVERFLOW          0
#define configUSE_MALLOC_FAILED_HOOK            0
#define configUSE_DAEMON_TASK_STARTUP_HOOK      0

/* Run time stats */
#define configGENERATE_RUN_TIME_STATS           0
#define configUSE_TRACE_FACILITY                0
#define configUSE_STATS_FORMATTING_FUNCTIONS    0

/* Task aware debugging */
#define configRECORD_STACK_HIGH_ADDRESS         1

/* Co-routines */
#define configUSE_CO_ROUTINES                   0
#define configMAX_CO_ROUTINE_PRIORITIES         2

/* Software timers */
#define configUSE_TIMERS                        1
#define configTIMER_TASK_PRIORITY               (configMAX_PRIORITIES - 2)
#define configTIMER_QUEUE_LENGTH                10
#define configTIMER_TASK_STACK_DEPTH            (configMINIMAL_STACK_SIZE * 2)

/* Assert */
#define configASSERT_DEFINED 1
#define configASSERT(x) if ((x) == 0) { taskDISABLE_INTERRUPTS(); for (;;) ; }

/* Optional functions */
#define INCLUDE_vTaskPrioritySet                1
#define INCLUDE_uxTaskPriorityGet               1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_vTaskDelayUntil                 1
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_xTaskGetSchedulerState          1
#define INCLUDE_xTaskGetCurrentTaskHandle       1
#define INCLUDE_uxTaskGetStackHighWaterMark     0
#define INCLUDE_xTaskGetIdleTaskHandle          0
#define INCLUDE_eTaskGetState                   0
#define INCLUDE_xTimerPendFunctionCall          1
#define INCLUDE_xTaskAbortDelay                 0
#define INCLUDE_xTaskGetHandle                  0
#define INCLUDE_xTaskResumeFromISR              1

/*-----------------------------------------------------------
 * AArch64-specific configuration
 *----------------------------------------------------------*/

/* GICv3 interrupt configuration */
#define configUNIQUE_INTERRUPT_PRIORITIES       256
#define configMAX_API_CALL_INTERRUPT_PRIORITY   192
#define configKERNEL_INTERRUPT_PRIORITY         255
#define portPRIORITY_SHIFT                      0   /* GICv3 priority is already in LSBs */

/* Tick interrupt setup - use our board driver */
extern void vSetupTickInterrupt(void);
#define configSETUP_TICK_INTERRUPT()            vSetupTickInterrupt()

extern void vClearTickInterrupt(void);
#define configCLEAR_TICK_INTERRUPT()            vClearTickInterrupt()

/* No MPU */
#define configENABLE_MPU                        0

/* Benchmark code does not use FP/SIMD registers on QEMU or RK3588 EL1 bring-up. */
#define configUSE_TASK_FPU_SUPPORT              1

/* Single core for now */
#ifndef configNUMBER_OF_CORES
#define configNUMBER_OF_CORES                   1
#endif

/* EL1 bare metal */
#define configRUN_IN_EL1                        1

/* Port optimised task selection */
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 1

#endif /* FREERTOS_CONFIG_H */
