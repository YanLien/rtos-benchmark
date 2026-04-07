/*
 * FreeRTOS AArch64 port for RK3588 (GICv3 system register interface)
 *
 * Adapted from FreeRTOS-Kernel portable/GCC/ARM_AARCH64/portmacro.h
 * Modified to use GICv3 ICC_*_EL1 system registers instead of MMIO.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef PORTMACRO_H
#define PORTMACRO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*-----------------------------------------------------------
 * Port specific definitions
 *----------------------------------------------------------*/

/* Type definitions - must match standard FreeRTOS expectations */
typedef long BaseType_t;
typedef unsigned long UBaseType_t;
typedef uint64_t StackType_t;

#if configUSE_16_BIT_TICKS == 1
    typedef uint16_t TickType_t;
    #define portMAX_DELAY    ( ( TickType_t ) 0xffff )
#else
    typedef uint32_t TickType_t;
    #define portMAX_DELAY    ( ( TickType_t ) 0xffffffffUL )
#endif

#define portSTACK_TYPE              uint64_t
#define portBASE_TYPE               long
#define portUBASE_TYPE              unsigned long

/* Architecture specifics */
#define portSTACK_GROWTH            ( -1 )
#define portTICK_PERIOD_MS          ( ( TickType_t ) 1000 / configTICK_RATE_HZ )
#define portBYTE_ALIGNMENT          16
#define portNOP()                   __asm__ volatile ( "NOP" )

/* 8-bit and 16-bit max values */
#define portMAX_8_BIT_VALUE         ( ( uint8_t ) 0xff )
#define portMAX_16_BIT_VALUE        ( ( uint16_t ) 0xffff )
#define portMAX_32_BIT_VALUE        ( ( uint32_t ) 0xffffffffUL )

/* GICv3 priority shift */
#define portPRIORITY_SHIFT          0

/* Priority definitions for GICv3 */
#define portLOWEST_USABLE_INTERRUPT_PRIORITY   ( configUNIQUE_INTERRUPT_PRIORITIES - 1 )
#define portLOWEST_INTERRUPT_PRIORITY          ( 255 )

/* Critical section management */
#define portDISABLE_INTERRUPTS()               \
{                                              \
    __asm__ volatile ( "MSR DAIFSET, #2" ::: "memory" ); \
    __asm__ volatile ( "DSB SY" ::: "memory" ); \
    __asm__ volatile ( "ISB SY" ::: "memory" ); \
}

#define portENABLE_INTERRUPTS()                \
{                                              \
    __asm__ volatile ( "MSR DAIFCLR, #2" ::: "memory" ); \
    __asm__ volatile ( "ISB SY" ::: "memory" ); \
}

/* Enter/exit critical sections */
extern void vPortEnterCritical(void);
extern void vPortExitCritical(void);
#define portENTER_CRITICAL()                   vPortEnterCritical()
#define portEXIT_CRITICAL()                    vPortExitCritical()

/* Set/clear interrupt mask (for FromISR variants) */
extern UBaseType_t uxPortSetInterruptMask(void);
extern void vPortClearInterruptMask(UBaseType_t uxNewMaskValue);
#define portSET_INTERRUPT_MASK_FROM_ISR()      uxPortSetInterruptMask()
#define portCLEAR_INTERRUPT_MASK_FROM_ISR(x)   vPortClearInterruptMask(x)

/* Task function macros */
#define portTASK_FUNCTION(vFunction, pvParameters) void vFunction(void *pvParameters)
#define portTASK_FUNCTION_PROTO(vFunction, pvParameters) void vFunction(void *pvParameters)

/* Task selection */
#if configUSE_PORT_OPTIMISED_TASK_SELECTION == 1
    #define portRECORD_READY_PRIORITY(uxPriority, uxReadyPriorities) \
        (uxReadyPriorities) |= (1UL << (uxPriority))
    #define portRESET_READY_PRIORITY(uxPriority, uxReadyPriorities) \
        (uxReadyPriorities) &= ~(1UL << (uxPriority))
    #define portGET_HIGHEST_PRIORITY(uxTopPriority, uxReadyPriorities) \
        uxTopPriority = (31 - __builtin_clz(uxReadyPriorities))
#endif

/* Yield */
extern uint64_t ullPortYieldRequired;
extern void vPortYield(void);
#define portYIELD()                 __asm__ volatile ( "svc #0" ::: "memory" )
#define portYIELD_WITHIN_API()      portYIELD()
#define portEND_SWITCHING_ISR(x)    do { if ((x) != pdFALSE) { ullPortYieldRequired = pdTRUE; } } while (0)

/* Scheduler utilities */
extern void vPortRestoreTaskContext(void);
#define portYIELD_FROM_ISR(x)       portEND_SWITCHING_ISR(x)

/* Tick type properties */
#define portTICK_TYPE_IS_ATOMIC     0

#ifdef __cplusplus
}
#endif

#endif /* PORTMACRO_H */
