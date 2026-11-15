; =============================================================
;  gdt_flush.asm - Load the new GDT and switch to its segments
;
;  void gdt_flush(uint32_t gdt_ptr_address);
;
;  lgdt only tells the CPU where the table is. The segment
;  registers still hold the OLD selectors (cached descriptors),
;  so we must reload every one of them.
; =============================================================
global gdt_flush

section .text
gdt_flush:
    mov eax, [esp + 4]      ; first argument: address of struct gdt_ptr
    lgdt [eax]              ; load GDTR register

    ; Data segment registers can be loaded with a normal mov
    mov ax, 0x10            ; kernel data selector (index 2 * 8)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; CS cannot be set with mov. A far jump loads CS:EIP together.
    jmp 0x08:.reload_cs     ; kernel code selector (index 1 * 8)
.reload_cs:
    ret
