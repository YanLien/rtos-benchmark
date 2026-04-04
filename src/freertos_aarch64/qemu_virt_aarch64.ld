/*
 * Linker script for FreeRTOS benchmark on QEMU virt (AArch64).
 *
 * Memory layout for QEMU virt machine:
 * - RAM starts at 0x40000000, QEMU loads kernel here
 * - Total region: 16 MB (enough for benchmark + stacks)
 */

ENTRY(Reset_Handler)

MEMORY
{
    RAM (rwx) : ORIGIN = 0x40000000, LENGTH = 16M
}

SECTIONS
{
    .text : {
        *(.text.boot)
        *(.text .text.*)
        *(.rodata .rodata.*)
    } > RAM

    .data : {
        . = ALIGN(8);
        *(.data .data.*)
        . = ALIGN(8);
    } > RAM

    .bss : {
        . = ALIGN(8);
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(8);
        __bss_end = .;
    } > RAM

    /* Stack and heap */
    .stack (NOLOAD) : {
        . = ALIGN(16);
        _stack_bottom = .;
        . += 65536;         /* 64 KB main stack */
        _stack_top = .;
    } > RAM

    /DISCARD/ : {
        *(.ARM.attributes)
        *(.comment)
        *(.note.*)
    }
}
