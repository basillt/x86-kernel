; =============================================================
;  boot.asm - Kernel entry point
;
;  GRUB (or QEMU -kernel) loads us at 1MB, already in 32-bit
;  protected mode, and jumps to _start. Our job here:
;    1. Provide a Multiboot header so the bootloader finds us
;    2. Set up a stack (C cannot run without one)
;    3. Call kernel_main() in C
; =============================================================

; ---- Multiboot header constants ----
MBALIGN  equ 1 << 0             ; align loaded modules on page boundaries
MEMINFO  equ 1 << 1             ; ask the bootloader for a memory map
FLAGS    equ MBALIGN | MEMINFO
MAGIC    equ 0x1BADB002         ; "magic number" the bootloader searches for
CHECKSUM equ -(MAGIC + FLAGS)   ; MAGIC + FLAGS + CHECKSUM must equal 0

; The header must be in the first 8KB of the kernel file,
; 4-byte aligned. linker.ld puts this section first.
section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM

; ---- Stack: 16KB reserved in .bss (uninitialized memory) ----
; The stack grows DOWN on x86, so esp starts at the top.
section .bss
align 16
stack_bottom:
    resb 16384
stack_top:

; ---- Entry point ----
section .text
global _start
extern kernel_main

_start:
    mov esp, stack_top          ; now C functions have a stack

    ; The bootloader leaves two useful values in registers:
    ;   eax = 0x2BADB002 (proof we were booted by a Multiboot loader)
    ;   ebx = address of the Multiboot info structure
    ; Push them (right-to-left, cdecl) as arguments to kernel_main.
    push ebx
    push eax
    call kernel_main

    ; If kernel_main ever returns, stop the CPU forever.
    cli                         ; disable interrupts
.hang:
    hlt                         ; halt until next interrupt (none will come)
    jmp .hang                   ; just in case (NMI can wake hlt)
