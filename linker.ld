/*
 * linker.ld - Tells the linker how to lay out the kernel in memory.
 */
ENTRY(_start)               /* execution starts at _start (boot.asm) */

SECTIONS
{
    /* Load the kernel at 1MB. Below that is BIOS/VGA/legacy stuff. */
    . = 1M;

    /* Multiboot header FIRST so it's within the first 8KB of the file */
    .text BLOCK(4K) : ALIGN(4K)
    {
        *(.multiboot)
        *(.text*)
    }

    .rodata BLOCK(4K) : ALIGN(4K)  { *(.rodata*) }   /* constants, strings */
    .data   BLOCK(4K) : ALIGN(4K)  { *(.data*) }     /* initialized globals */
    .bss    BLOCK(4K) : ALIGN(4K)  { *(COMMON) *(.bss*) }  /* zeroed globals + stack */

    /* Sections the host compiler adds that we don't need */
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
