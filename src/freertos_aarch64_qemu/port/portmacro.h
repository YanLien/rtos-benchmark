/*
 * FreeRTOS AArch64 SMP port for QEMU virt (Cortex-A55, GICv3)
 *
 * Adapted from FreeRTOS-Kernel portable/GCC/ARM_AARCH64/portmacro.h
 * and portable/GCC/ARM_CR82/portmacro.h for SMP support.
 * Uses GICv3 ICC_*_EL1 system registers and SGI for IPI.
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

/* SGI interrupt ID used for inter-core yield */
#define portYIELD_CORE_INT_ID      ( 0x0U )

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

#if ( configNUMBER_OF_CORES == 1 )
    /* Single-core critical sections */
    extern void vPortEnterCritical(void);
    extern void vPortExitCritical(void);
    #define portENTER_CRITICAL()                   vPortEnterCritical()
    #define portEXIT_CRITICAL()                    vPortExitCritical()
#else
    /* SMP: use kernel-provided critical sections */
    #define portENTER_CRITICAL()                   vTaskEnterCritical()
    #define portEXIT_CRITICAL()                    vTaskExitCritical()
#endif

/* Set/clear interrupt mask (for FromISR variants) */
extern UBaseType_t uxPortSetInterruptMask(void);
extern void vPortClearInterruptMask(UBaseType_t uxNewMaskValue);
#define portSET_INTERRUPT_MASK_FROM_ISR()      uxPortSetInterruptMask()
#define portCLEAR_INTERRUPT_MASK_FROM_ISR(x)   vPortClearInterruptMask(x)

/* Task function macros */
#define portTASK_FUNCTION(vFunction, pvParameters) void vFunction(void *pvParameters)
#define portTASK_FUNCTION_PROTO(vFunction, pvParameters) void vFunction(void *pvParameters)

/* Task selection - disabled for SMP (configUSE_PORT_OPTIMISED_TASK_SELECTION=0) */
#if configUSE_PORT_OPTIMISED_TASK_SELECTION == 1
    #define portRECORD_READY_PRIORITY(uxPriority, uxReadyPriorities) \
        (uxReadyPriorities) |= (1UL << (uxPriority))
    #define portRESET_READY_PRIORITY(uxPriority, uxReadyPriorities) \
        (uxReadyPriorities) &= ~(1UL << (uxPriority))
    #define portGET_HIGHEST_PRIORITY(uxTopPriority, uxReadyPriorities) \
        uxTopPriority = (31 - __builtin_clz(uxReadyPriorities))
#endif

/* Yield */
#define portYIELD()                 __asm__ volatile ( "svc #0" ::: "memory" )
#define portYIELD_WITHIN_API()      portYIELD()

/* Scheduler utilities */
extern void vPortRestoreTaskContext(void);

/* Tick type properties */
#define portTICK_TYPE_IS_ATOMIC     0

/*-----------------------------------------------------------
 * SMP-specific definitions (configNUMBER_OF_CORES > 1)
 *----------------------------------------------------------*/

#if ( configNUMBER_OF_CORES > 1 )

    #if !defined(__ASSEMBLER__)
        typedef enum
        {
            eIsrLock = 0,
            eTaskLock,
            eLockCount
        } ePortRTOSLock;

        extern volatile uint64_t ullCriticalNestings[ configNUMBER_OF_CORES ];
        extern volatile uint64_t ullPortTaskHasFPUContext[ configNUMBER_OF_CORES ];
        extern volatile uint64_t ullPortYieldRequired[ configNUMBER_OF_CORES ];
        extern volatile uint64_t ullPortInterruptNestings[ configNUMBER_OF_CORES ];

        extern void vPortRecursiveLock( uint8_t ucCoreID,
                                        ePortRTOSLock eLockNum,
                                        BaseType_t uxAcquire );
        extern void vInterruptCore( uint32_t ulInterruptID, uint8_t ucCoreID );
        extern uint8_t ucPortGetCoreID(void);
        extern uint8_t ucPortGetCoreIDFromIsr(void);

        /* Secondary core entry (called from startup_aarch64.S) */
        extern void vPortSecondaryEntry(void);

        /* Secondary core ready flags (bitmask, set from ASM) */
        extern volatile uint8_t ucSecondaryCoresReadyFlags;

        /* Scheduler started flags - primary sets per-core bit before
         * secondary core enters vPortRestoreTaskContext */
        extern volatile uint8_t ucSchedulerStartedFlags;

        /* Yield switching for SMP */
        #define portEND_SWITCHING_ISR( xSwitchRequired )                   \
        {                                                                  \
            if( xSwitchRequired != pdFALSE )                               \
            {                                                              \
                ullPortYieldRequired[ portGET_CORE_ID() ] = pdTRUE;        \
            }                                                              \
        }
        #define portYIELD_FROM_ISR( x )    portEND_SWITCHING_ISR( x )
    #endif /* !__ASSEMBLER__ */

    #define portSET_INTERRUPT_MASK()         uxPortSetInterruptMask()
    #define portCLEAR_INTERRUPT_MASK( x )    vPortClearInterruptMask( x )

    #define portMAX_CORE_COUNT               configNUMBER_OF_CORES
    #define portGET_CORE_ID()                ucPortGetCoreID()
    #define portGET_CORE_ID_FROM_ISR()       ucPortGetCoreIDFromIsr()

    /* Use SGI 0 as the yield core interrupt */
    #define portYIELD_CORE( xCoreID )                          \
        vInterruptCore( portYIELD_CORE_INT_ID, ( uint8_t ) xCoreID )

    #define portRELEASE_ISR_LOCK( xCoreID )                    \
        vPortRecursiveLock( ( uint8_t ) xCoreID, eIsrLock, pdFALSE )
    #define portGET_ISR_LOCK( xCoreID )                        \
        vPortRecursiveLock( ( uint8_t ) xCoreID, eIsrLock, pdTRUE )

    #define portRELEASE_TASK_LOCK( xCoreID )                   \
        vPortRecursiveLock( ( uint8_t ) xCoreID, eTaskLock, pdFALSE )
    #define portGET_TASK_LOCK( xCoreID )                       \
        vPortRecursiveLock( ( uint8_t ) xCoreID, eTaskLock, pdTRUE )

    #define portGET_CRITICAL_NESTING_COUNT( xCoreID )          \
        ( ullCriticalNestings[ ( uint8_t ) xCoreID ] )
    #define portSET_CRITICAL_NESTING_COUNT( xCoreID, x )       \
        ( ullCriticalNestings[ ( uint8_t ) xCoreID ] = ( x ) )
    #define portINCREMENT_CRITICAL_NESTING_COUNT( xCoreID )    \
        ( ullCriticalNestings[ ( uint8_t ) xCoreID ]++ )
    #define portDECREMENT_CRITICAL_NESTING_COUNT( xCoreID )    \
        ( ullCriticalNestings[ ( uint8_t ) xCoreID ]-- )

    /* ISR critical sections delegate to kernel SMP-aware versions */
    extern UBaseType_t vTaskEnterCriticalFromISR(void);
    extern void vTaskExitCriticalFromISR(UBaseType_t uxSavedInterruptStatus);
    #define portENTER_CRITICAL_FROM_ISR()             vTaskEnterCriticalFromISR()
    #define portEXIT_CRITICAL_FROM_ISR( x )           vTaskExitCriticalFromISR( x )

#else
    /* Single-core yield/switching */
    extern uint64_t ullPortYieldRequired;
    #define portEND_SWITCHING_ISR(x)    \
        do { if ((x) != pdFALSE) { ullPortYieldRequired = pdTRUE; } } while (0)
    #define portYIELD_FROM_ISR(x)       portEND_SWITCHING_ISR(x)
#endif /* configNUMBER_OF_CORES > 1 */

#ifdef __cplusplus
}
#endif

#endif /* PORTMACRO_H */
