; =============================================================
;  interrupts.asm - Entry points for exceptions and IRQs
;
;  When an interrupt fires, the CPU pushes EFLAGS, CS and EIP
;  (and, for SOME exceptions, an error code), then jumps to the
;  address in the IDT. It does NOT tell us which vector it was.
;
;  So every vector gets its own tiny stub that:
;    1. pushes a dummy error code if the CPU didn't push one,
;       so the stack always looks the same
;    2. pushes its own vector number
;    3. jumps to one shared routine that saves everything and
;       calls into C
; =============================================================

extern interrupt_dispatch

; ---- Stub for exceptions WITHOUT a CPU error code ----
%macro ISR_NOERR 1
isr%1:
    push dword 0            ; dummy error code
    push dword %1           ; vector number
    jmp interrupt_common
%endmacro

; ---- Stub for exceptions WITH a CPU error code ----
%macro ISR_ERR 1
isr%1:
    ; error code already on the stack (pushed by the CPU)
    push dword %1
    jmp interrupt_common
%endmacro

; ---- Stub for hardware IRQs (vectors 32-47) ----
%macro IRQ 2
isr%2:
    push dword 0
    push dword %2
    jmp interrupt_common
%endmacro

section .text

; Exceptions 8, 10-14, 17, 21, 29 and 30 push an error code.
ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_ERR   21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_ERR   29
ISR_ERR   30
ISR_NOERR 31

; IRQ 0-15 -> vectors 32-47
IRQ 0, 32
IRQ 1, 33
IRQ 2, 34
IRQ 3, 35
IRQ 4, 36
IRQ 5, 37
IRQ 6, 38
IRQ 7, 39
IRQ 8, 40
IRQ 9, 41
IRQ 10, 42
IRQ 11, 43
IRQ 12, 44
IRQ 13, 45
IRQ 14, 46
IRQ 15, 47

; ---- Shared part: save state, call C, restore, return ----
interrupt_common:
    pusha                   ; eax, ecx, edx, ebx, esp, ebp, esi, edi
    push ds
    push es
    push fs
    push gs

    mov ax, 0x10            ; make sure we use the kernel data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    cld                     ; C code expects the direction flag cleared
    push esp                ; argument: pointer to the saved registers
    call interrupt_dispatch
    add esp, 4

    pop gs
    pop fs
    pop es
    pop ds
    popa
    add esp, 8              ; drop vector number + error code
    iret                    ; pops EIP, CS, EFLAGS and resumes

; ---- Table of stub addresses, used by idt.c ----
section .data
global isr_stub_table
isr_stub_table:
%assign i 0
%rep 48
    dd isr %+ i
%assign i i+1
%endrep
