/*
 * FreeRTOS AArch64 port for RK3588 (GICv3 system register interface)
 *
 * Context switching and interrupt handling for ARMv8-A with GICv3.
 * Uses ICC_*_EL1 system registers for GIC access.
 *
 * SPDX-License-Identifier: MIT
 */

.text
.arch armv8-a

.extern ullCriticalNesting
.extern ullPortTaskHasFPUContext
.extern ullPortYieldRequired
.extern ullPortInterruptNesting
.extern pxCurrentTCB
.extern vTaskIncrementTick
.extern vTaskSwitchContext
.extern vApplicationIRQHandler
.extern FreeRTOS_Tick_Handler

.extern ullICCIARValue

/* GICv3 EL1 Physical Timer interrupt ID (PPI 30) */
.equ TIMER_EL1_IRQ, 30

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
 * Save all general purpose registers except x0 and x1.
 * Assumes x1 points to the top of the stack.
 */
.macro saveallgpregisters
    /* x0 and x1 are handled separately */
    stp     x2, x3, [x1, #-16]!
    stp     x4, x5, [x1, #-16]!
    stp     x6, x7, [x1, #-16]!
    stp     x8, x9, [x1, #-16]!
    stp     x10, x11, [x1, #-16]!
    stp     x12, x13, [x1, #-16]!
    stp     x14, x15, [x1, #-16]!
    stp     x16, x17, [x1, #-16]!
    stp     x18, x19, [x1, #-16]!
    stp     x20, x21, [x1, #-16]!
    stp     x22, x23, [x1, #-16]!
    stp     x24, x25, [x1, #-16]!
    stp     x26, x27, [x1, #-16]!
    stp     x28, x29, [x1, #-16]!
    stp     x30, xzr, [x1, #-16]!
.endm

/*-----------------------------------------------------------*/

/*
 * Restore all general purpose registers.
 * Assumes x1 points to the bottom of the saved register area.
 */
.macro restoreallgpregisters
    ldp     x30, xzr, [x1], #16
    ldp     x28, x29, [x1], #16
    ldp     x26, x27, [x1], #16
    ldp     x24, x25, [x1], #16
    ldp     x22, x23, [x1], #16
    ldp     x20, x21, [x1], #16
    ldp     x18, x19, [x1], #16
    ldp     x16, x17, [x1], #16
    ldp     x14, x15, [x1], #16
    ldp     x12, x13, [x1], #16
    ldp     x10, x11, [x1], #16
    ldp     x8, x9, [x1], #16
    ldp     x6, x7, [x1], #16
    ldp     x4, x5, [x1], #16
    ldp     x2, x3, [x1], #16
    ldp     x0, x1, [x1], #16
.endm

/*-----------------------------------------------------------*/

/*
 * Save the current task context to the stack.
 */
.macro portSAVE_CONTEXT
    /* Save the current SP to x0, then save x0 and x1 to the stack */
    mov     x0, sp
    stp     x0, x1, [sp, #-16]!
    mov     x1, sp

    /* Save remaining GP registers */
    saveallgpregisters

    /* Save SPSR and ELR (return address) */
    mrs     x2, spsr_el1
    mrs     x3, elr_el1
    stp     x3, x2, [x1, #-16]!

    /* Save critical nesting count */
    ldr     x2, =ullCriticalNesting
    ldr     x2, [x2]
    str     x2, [x1, #-8]!

    /* Save FPU context flag */
    ldr     x2, =ullPortTaskHasFPUContext
    ldr     x2, [x2]
    str     x2, [x1, #-8]!

    /* If FPU context is present, save FPU registers */
    cbz     x2, 1f
    mov     x0, x1
    sub     x1, x1, #(32 * 16)    /* Space for 32 x 128-bit Q registers */
    savefloatregisters
1:
    /* Store the new top of stack in the TCB */
    ldr     x2, =pxCurrentTCB
    ldr     x2, [x2]
    str     x1, [x2]
.endm

/*-----------------------------------------------------------*/

/*
 * Restore a task context from the stack.
 */
.macro portRESTORE_CONTEXT
    /* Get the top of stack from the TCB */
    ldr     x0, =pxCurrentTCB
    ldr     x0, [x0]
    ldr     x1, [x0]

    /* Check if FPU context is present */
    ldr     x2, [x1], #8
    ldr     x3, =ullPortTaskHasFPUContext
    str     x2, [x3]

    cbz     x2, 1f
    /* Restore FPU registers */
    mov     x0, x1
    restorefloatregisters
    mov     x1, x0
1:
    /* Restore critical nesting count */
    ldr     x2, [x1], #8
    ldr     x3, =ullCriticalNesting
    str     x2, [x3]

    /* Restore ELR and SPSR */
    ldp     x3, x2, [x1], #16
    msr     elr_el1, x3
    msr     spsr_el1, x2

    /* Restore GP registers */
    restoreallgpregisters

    /* Restore SP */
    ldr     x0, [sp]
    mov     sp, x0

    /* Ensure memory ordering */
    dsb     sy
    isb     sy

    eret
.endm

/*-----------------------------------------------------------*/

/*
 * vPortYield - yield the current task.
 */
.global vPortYield
vPortYield:
    portSAVE_CONTEXT
    bl      vTaskSwitchContext
    portRESTORE_CONTEXT

/*-----------------------------------------------------------*/

/*
 * FreeRTOS_IRQ_Handler - called when an IRQ occurs.
 * Uses GICv3 system register interface (ICC_IAR1_EL1 / ICC_EOIR1_EL1).
 */
.global FreeRTOS_IRQ_Handler
FreeRTOS_IRQ_Handler:
    /* Save context */
    portSAVE_CONTEXT

    /* Increment nesting count */
    ldr     x0, =ullPortInterruptNesting
    ldr     x1, [x0]
    add     x1, x1, #1
    str     x1, [x0]

    /* Read interrupt ID from GICv3 (system register) */
    mrs     x0, ICC_IAR1_EL1
    dsb     sy

    /* Store the IRQ ID for the C handler */
    ldr     x1, =ullICCIARValue
    str     x0, [x1]

    /* Acknowledge the interrupt with GIC */
    msr     ICC_EOIR1_EL1, x0
    isb

    /* Call the application IRQ handler with the IRQ ID */
    bl      vApplicationIRQHandler

    /* Decrement nesting count */
    ldr     x0, =ullPortInterruptNesting
    ldr     x1, [x0]
    sub     x1, x1, #1
    str     x1, [x0]

    /* Check if context switch is needed */
    ldr     x0, =ullPortYieldRequired
    ldr     x1, [x0]
    cbz     x1, 1f
    mov     x2, #0
    str     x2, [x0]
    bl      vTaskSwitchContext

1:
    portRESTORE_CONTEXT

/*-----------------------------------------------------------*/

/*
 * vPortRestoreTaskContext - start the first task.
 */
.global vPortRestoreTaskContext
vPortRestoreTaskContext:
    portRESTORE_CONTEXT

/*-----------------------------------------------------------*/

.end
