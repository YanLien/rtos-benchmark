/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * AArch64 startup code for RK3588 FreeRTOS benchmark.
 * Runs bare-metal on one Cortex-A76 core in AMP mode.
 *
 * Exception vector table layout (each entry 0x80 bytes apart):
 *   +0x000: Synchronous (Current EL, SP0)
 *   +0x080: IRQ        (Current EL, SP0)
 *   +0x100: FIQ        (Current EL, SP0)
 *   +0x180: SError     (Current EL, SP0)
 *   +0x200: Synchronous (Current EL, SPx)
 *   +0x280: IRQ        (Current EL, SPx) <-- FreeRTOS uses this
 *   +0x300: FIQ        (Current EL, SPx)
 *   +0x380: SError     (Current EL, SPx)
 */

.section .text.boot
.global Reset_Handler
.global _vector_table

.extern FreeRTOS_IRQ_Handler
.extern FreeRTOS_SWI_Handler

/*
 * Exception vector table for EL1.
 * Each entry is 0x80 bytes (max 32 instructions).
 */
.align 11
_vector_table:
    /* Current EL with SP0 - tasks run in EL1t, so yield/IRQ arrive here */
    b   FreeRTOS_SWI_Handler       /* +0x000 Synchronous */
    .align 7
    b   FreeRTOS_IRQ_Handler       /* +0x080 IRQ */
    .align 7
    b   hang                       /* +0x100 FIQ */
    .align 7
    b   hang                       /* +0x180 SError */
    .align 7

    /* Current EL with SPx - keep these routed too for debug/bring-up */
    b   FreeRTOS_SWI_Handler       /* +0x200 Synchronous */
    .align 7
    b   FreeRTOS_IRQ_Handler       /* +0x280 IRQ */
    .align 7
    b   hang                       /* +0x300 FIQ */
    .align 7
    b   hang                       /* +0x380 SError */
    .align 7

    /* Lower EL using AArch64 - not used in bare metal */
    b   hang
    .align 7
    b   hang
    .align 7
    b   hang
    .align 7
    b   hang
    .align 7

    /* Lower EL using AArch32 - not used */
    b   hang
    .align 7
    b   hang
    .align 7
    b   hang
    .align 7
    b   hang
    .align 7


/*
 * Reset handler - entry point for secondary core boot.
 */
.align 3
Reset_Handler:
    /* Check current EL */
    mrs     x0, CurrentEL
    lsr     x0, x0, #2
    cmp     x0, #3
    b.eq    el3_start
    cmp     x0, #2
    b.eq    el2_start
    b       el1_start

el3_start:
    /* Running at EL3 - drop to EL1 */
    msr     sctlr_el1, xzr
    isb

    ldr     x0, =_vector_table
    msr     vbar_el1, x0
    msr     hcr_el2, xzr

    mov     x0, #0x1         /* NS bit */
    msr     scr_el3, x0

    mov     x0, #0x3c5       /* DAIF masked, EL1h */
    msr     spsr_el3, x0
    adr     x0, el1_start
    msr     elr_el3, x0
    eret

el2_start:
    /* Running at EL2 (e.g. u-boot go) - drop to EL1 */
    msr     sctlr_el1, xzr
    isb

    ldr     x0, =_vector_table
    msr     vbar_el1, x0

    /* Set HCR_EL2: route FIQ/IRQ to EL1, disable EL2 traps */
    mov     x0, #(1 << 27)   /* TSC: trap SMC to EL1 is fine */
    msr     hcr_el2, x0
    isb

    /* Return to EL1h with all interrupts masked */
    mov     x0, #0x3c5       /* DAIF masked, EL1h (SP = SP_EL1) */
    msr     spsr_el2, x0
    adr     x0, el1_start
    msr     elr_el2, x0
    eret

el1_start:
    /* Running at EL1 */
    ldr     x0, =_vector_table
    msr     vbar_el1, x0
    isb

    /* Set up stack pointer */
    ldr     x0, =_stack_top
    mov     sp, x0

    /* Clear BSS */
    ldr     x0, =__bss_start
    ldr     x1, =__bss_end
    cmp     x0, x1
    b.ge    2f
    mov     x2, #0
clear_bss:
    str     x2, [x0], #8
    cmp     x0, x1
    b.lt    clear_bss

2:
    /* Invalidate caches */
    ic      iallu
    isb

    /* Jump to C entry point */
    bl      main

    /* Should not return */
hang:
    wfe
    b       hang


/*
 * Stack area.
 * Each benchmark task has its own stack via static allocation,
 * but we need an initial stack for startup.
 */
.section .bss
.align 12
.global _stack_bottom
_stack_bottom:
    .skip   65536       /* 64 KB startup stack */
.global _stack_top
_stack_top:
