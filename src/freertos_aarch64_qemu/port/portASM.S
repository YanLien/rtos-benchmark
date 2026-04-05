/*
 * FreeRTOS AArch64 SMP port for QEMU virt (Cortex-A55, GICv3)
 *
 * Context switching and interrupt handling for ARMv8-A with GICv3.
 * Uses ICC_*_EL1 system registers for GIC access.
 * Supports multi-core SMP with per-core TCB arrays.
 *
 * Context layout on each task's SP_EL0 stack (from top/low addr to bottom/high addr):
 *   FPU flag
 *   critical nesting
 *   SPSR_EL1, ELR_EL1
 *   X30, XZR
 *   X28, X29
 *   ...
 *   X2, X3
 *   X0, X1          (task's actual register values)
 *
 * SPDX-License-Identifier: MIT
 */

.text
.arch armv8-a

#if ( configNUMBER_OF_CORES == 1 )
    .extern ullCriticalNesting
    .extern ullPortTaskHasFPUContext
    .extern ullPortYieldRequired
    .extern ullPortInterruptNesting
#else
    .extern ullCriticalNestings
    .extern ullPortTaskHasFPUContext
    .extern ullPortYieldRequired
    .extern ullPortInterruptNestings
#endif
.extern pxCurrentTCBs
.extern vTaskIncrementTick
.extern vTaskSwitchContext
.extern vApplicationIRQHandler
.extern FreeRTOS_Tick_Handler
.extern FreeRTOS_SGI_Handler

.extern ullICCIARValue
.extern ullMaxAPIPriorityMask

/* GICv3 EL1 Physical Timer interrupt ID (PPI 30) */
.equ TIMER_EL1_IRQ, 30
/* SGI 0 is used for inter-core yield */
.equ SGI_YIELD_IRQ, 0

/*-----------------------------------------------------------*/

/*
 * Save FPU registers (Q0-Q31 = 32 x 128-bit) to the task stack.
 * Assumes x0 points to the save area.
 */
.macro savefloatregisters
    stp     q0, q1, [x0], #32
    stp     q2, q3, [x0], #32
    stp     q4, q5, [x0], #32
    stp     q6, q7, [x0], #32
    stp     q8, q9, [x0], #32
    stp     q10, q11, [x0], #32
    stp     q12, q13, [x0], #32
    stp     q14, q15, [x0], #32
    stp     q16, q17, [x0], #32
    stp     q18, q19, [x0], #32
    stp     q20, q21, [x0], #32
    stp     q22, q23, [x0], #32
    stp     q24, q25, [x0], #32
    stp     q26, q27, [x0], #32
    stp     q28, q29, [x0], #32
    stp     q30, q31, [x0], #32
.endm

/*-----------------------------------------------------------*/

/*
 * Restore FPU registers from the task stack.
 * Assumes x0 points to the save area.
 */
.macro restorefloatregisters
    ldp     q0, q1, [x0], #32
    ldp     q2, q3, [x0], #32
    ldp     q4, q5, [x0], #32
    ldp     q6, q7, [x0], #32
    ldp     q8, q9, [x0], #32
    ldp     q10, q11, [x0], #32
    ldp     q12, q13, [x0], #32
    ldp     q14, q15, [x0], #32
    ldp     q16, q17, [x0], #32
    ldp     q18, q19, [x0], #32
    ldp     q20, q21, [x0], #32
    ldp     q22, q23, [x0], #32
    ldp     q24, q25, [x0], #32
    ldp     q26, q27, [x0], #32
    ldp     q28, q29, [x0], #32
    ldp     q30, q31, [x0], #32
.endm

/*-----------------------------------------------------------*/

/*
 * Get the current core ID from MPIDR_EL1 (Aff0 field).
 * Result in register \rd.
 */
.macro get_core_id rd
    mrs     \rd, MPIDR_EL1
    and     \rd, \rd, #0xff
.endm

/*-----------------------------------------------------------*/

/*
 * Save the current task context to the task stack (SP_EL0).
 * SMP version: uses pxCurrentTCBs[coreID] and per-core arrays.
 */
.macro portSAVE_CONTEXT
    /* Switch to use the EL0 stack pointer. */
    msr     SPSEL, #0

    /* Save the full general-purpose register state. */
    stp     x0, x1, [sp, #-0x10]!
    stp     x2, x3, [sp, #-0x10]!
    stp     x4, x5, [sp, #-0x10]!
    stp     x6, x7, [sp, #-0x10]!
    stp     x8, x9, [sp, #-0x10]!
    stp     x10, x11, [sp, #-0x10]!
    stp     x12, x13, [sp, #-0x10]!
    stp     x14, x15, [sp, #-0x10]!
    stp     x16, x17, [sp, #-0x10]!
    stp     x18, x19, [sp, #-0x10]!
    stp     x20, x21, [sp, #-0x10]!
    stp     x22, x23, [sp, #-0x10]!
    stp     x24, x25, [sp, #-0x10]!
    stp     x26, x27, [sp, #-0x10]!
    stp     x28, x29, [sp, #-0x10]!
    stp     x30, xzr, [sp, #-0x10]!

    /* Save exception state. */
    mrs     x3, SPSR_EL1
    mrs     x2, ELR_EL1
    stp     x2, x3, [sp, #-0x10]!

    /* Get core ID */
    get_core_id x4

#if ( configNUMBER_OF_CORES > 1 )
    /* Save critical nesting and FPU indicator (per-core arrays). */
    ldr     x0, =ullCriticalNestings
    lsl     x5, x4, #3            /* x5 = coreID * 8 */
    ldr     x3, [x0, x5]
    ldr     x0, =ullPortTaskHasFPUContext
    ldr     x2, [x0, x5]
#else
    ldr     x0, =ullCriticalNesting
    ldr     x3, [x0]
    ldr     x0, =ullPortTaskHasFPUContext
    ldr     x2, [x0]
#endif

    cbz     x2, 1f
    stp     q0, q1, [sp, #-0x20]!
    stp     q2, q3, [sp, #-0x20]!
    stp     q4, q5, [sp, #-0x20]!
    stp     q6, q7, [sp, #-0x20]!
    stp     q8, q9, [sp, #-0x20]!
    stp     q10, q11, [sp, #-0x20]!
    stp     q12, q13, [sp, #-0x20]!
    stp     q14, q15, [sp, #-0x20]!
    stp     q16, q17, [sp, #-0x20]!
    stp     q18, q19, [sp, #-0x20]!
    stp     q20, q21, [sp, #-0x20]!
    stp     q22, q23, [sp, #-0x20]!
    stp     q24, q25, [sp, #-0x20]!
    stp     q26, q27, [sp, #-0x20]!
    stp     q28, q29, [sp, #-0x20]!
    stp     q30, q31, [sp, #-0x20]!
1:
    stp     x2, x3, [sp, #-0x10]!

    /* Save the new task-stack top in the TCB. */
#if ( configNUMBER_OF_CORES > 1 )
    ldr     x0, =pxCurrentTCBs
    lsl     x5, x4, #3            /* x5 = coreID * 8 */
    ldr     x1, [x0, x5]
#else
    ldr     x0, =pxCurrentTCB
    ldr     x1, [x0]
#endif
    mov     x0, sp
    str     x0, [x1]

    /* Switch back to the EL1 exception stack. */
    msr     SPSEL, #1
.endm

/*-----------------------------------------------------------*/

/*
 * Restore a task context from the task stack (SP_EL0).
 * SMP version: uses pxCurrentTCBs[coreID] and per-core arrays.
 */
.macro portRESTORE_CONTEXT
    /* Switch to use the EL0 stack pointer. */
    msr     SPSEL, #0

    /* Get core ID */
    get_core_id x4

    /* Set SP to the task stack recorded in the TCB. */
#if ( configNUMBER_OF_CORES > 1 )
    ldr     x0, =pxCurrentTCBs
    lsl     x5, x4, #3            /* x5 = coreID * 8 */
    ldr     x1, [x0, x5]
#else
    ldr     x0, =pxCurrentTCB
    ldr     x1, [x0]
#endif
    ldr     x0, [x1]
    mov     sp, x0

    /* Restore critical nesting and FPU indicator. */
    ldp     x2, x3, [sp], #0x10

    /* Program PMR according to the restored critical nesting depth. */
    mov     x1, #255
    cmp     x3, #0
    b.eq    1f
    ldr     x6, =ullMaxAPIPriorityMask
    ldr     x1, [x6]
1:
    msr     ICC_PMR_EL1, x1
    dsb     sy
    isb     sy

#if ( configNUMBER_OF_CORES > 1 )
    ldr     x0, =ullCriticalNestings
    lsl     x5, x4, #3
    str     x3, [x0, x5]
    ldr     x0, =ullPortTaskHasFPUContext
    str     x2, [x0, x5]
#else
    ldr     x0, =ullCriticalNesting
    str     x3, [x0]
    ldr     x0, =ullPortTaskHasFPUContext
    str     x2, [x0]
#endif

    cbz     x2, 2f
    ldp     q30, q31, [sp], #0x20
    ldp     q28, q29, [sp], #0x20
    ldp     q26, q27, [sp], #0x20
    ldp     q24, q25, [sp], #0x20
    ldp     q22, q23, [sp], #0x20
    ldp     q20, q21, [sp], #0x20
    ldp     q18, q19, [sp], #0x20
    ldp     q16, q17, [sp], #0x20
    ldp     q14, q15, [sp], #0x20
    ldp     q12, q13, [sp], #0x20
    ldp     q10, q11, [sp], #0x20
    ldp     q8, q9, [sp], #0x20
    ldp     q6, q7, [sp], #0x20
    ldp     q4, q5, [sp], #0x20
    ldp     q2, q3, [sp], #0x20
    ldp     q0, q1, [sp], #0x20
2:
    /* Restore exception state. */
    ldp     x2, x3, [sp], #0x10
    msr     ELR_EL1, x2
    msr     SPSR_EL1, x3

    /* Restore general-purpose registers. */
    ldp     x30, xzr, [sp], #0x10
    ldp     x28, x29, [sp], #0x10
    ldp     x26, x27, [sp], #0x10
    ldp     x24, x25, [sp], #0x10
    ldp     x22, x23, [sp], #0x10
    ldp     x20, x21, [sp], #0x10
    ldp     x18, x19, [sp], #0x10
    ldp     x16, x17, [sp], #0x10
    ldp     x14, x15, [sp], #0x10
    ldp     x12, x13, [sp], #0x10
    ldp     x10, x11, [sp], #0x10
    ldp     x8, x9, [sp], #0x10
    ldp     x6, x7, [sp], #0x10
    ldp     x4, x5, [sp], #0x10
    ldp     x2, x3, [sp], #0x10
    ldp     x0, x1, [sp], #0x10

    msr     SPSEL, #1
    eret
.endm

/*-----------------------------------------------------------*/

/*
 * FreeRTOS_SWI_Handler - handles synchronous yields triggered by SVC.
 */
.global FreeRTOS_SWI_Handler
FreeRTOS_SWI_Handler:
    portSAVE_CONTEXT

    mrs     x0, ESR_EL1
    lsr     x1, x0, #26
    cmp     x1, #0x15              /* EC = SVC64 */
    b.ne    .

#if ( configNUMBER_OF_CORES > 1 )
    get_core_id x0
    bl      vTaskSwitchContext
#else
    bl      vTaskSwitchContext
#endif
    portRESTORE_CONTEXT

/*-----------------------------------------------------------*/

/*
 * vPortYield - raise an SVC to request a context switch.
 */
.global vPortYield
vPortYield:
    svc     #0
    ret

/*-----------------------------------------------------------*/

/*
 * FreeRTOS_IRQ_Handler - called when an IRQ occurs.
 * Uses GICv3 system register interface (ICC_IAR1_EL1 / ICC_EOIR1_EL1).
 */
.global FreeRTOS_IRQ_Handler
FreeRTOS_IRQ_Handler:
    /* Save context */
    portSAVE_CONTEXT

    /* Get core ID */
    get_core_id x4

    /* Increment nesting count (per-core) */
#if ( configNUMBER_OF_CORES > 1 )
    ldr     x0, =ullPortInterruptNestings
    lsl     x5, x4, #3
    add     x0, x0, x5
#else
    ldr     x0, =ullPortInterruptNesting
#endif
    ldr     x1, [x0]
    add     x1, x1, #1
    str     x1, [x0]

    /* Read interrupt ID from GICv3 (system register) */
    mrs     x0, ICC_IAR1_EL1
    dsb     sy

    /* Store the IRQ ID for the C handler */
    ldr     x1, =ullICCIARValue
    str     x0, [x1]

    /* Check if this is the timer IRQ or an SGI */
    cmp     x0, #TIMER_EL1_IRQ
    b.eq    irq_tick
    cmp     x0, #SGI_YIELD_IRQ
    b.lo    irq_eoi                 /* Unknown IRQ, just EOI */

    /* SGI (0-15): call the SGI handler */
    bl      FreeRTOS_SGI_Handler
    b       irq_eoi

irq_tick:
    /* Call the application IRQ handler with the IRQ ID */
    bl      vApplicationIRQHandler

irq_eoi:
    /* Acknowledge the interrupt with GIC after servicing the source */
    ldr     x1, =ullICCIARValue
    ldr     x0, [x1]
    msr     ICC_EOIR1_EL1, x0
    isb

    /* Decrement nesting count */
#if ( configNUMBER_OF_CORES > 1 )
    ldr     x0, =ullPortInterruptNestings
    lsl     x5, x4, #3
    add     x0, x0, x5
#else
    ldr     x0, =ullPortInterruptNesting
#endif
    ldr     x1, [x0]
    sub     x1, x1, #1
    str     x1, [x0]

    /* Check if context switch is needed */
#if ( configNUMBER_OF_CORES > 1 )
    ldr     x0, =ullPortYieldRequired
    lsl     x5, x4, #3
    add     x0, x0, x5
#else
    ldr     x0, =ullPortYieldRequired
#endif
    ldr     x1, [x0]
    cbz     x1, 1f
    mov     x2, #0
    str     x2, [x0]

#if ( configNUMBER_OF_CORES > 1 )
    mov     x0, x4                 /* pass coreID to vTaskSwitchContext */
#endif
    bl      vTaskSwitchContext

1:
    portRESTORE_CONTEXT

/*-----------------------------------------------------------*/

/*
 * vPortRestoreTaskContext - start the first task on a core.
 */
.global vPortRestoreTaskContext
vPortRestoreTaskContext:
    portRESTORE_CONTEXT

/*-----------------------------------------------------------*/

.end
