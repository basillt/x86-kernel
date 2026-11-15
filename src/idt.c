/*
 * idt.c - Interrupt Descriptor Table
 *
 * 256 entries, one per interrupt vector. Each entry ("gate") tells the
 * CPU which code segment and address to jump to when that interrupt
 * happens.
 *
 *   vectors  0-31   CPU exceptions (divide by zero, page fault, ...)
 *   vectors 32-47   hardware IRQs, after we remap the PIC (part 4)
 */
#include "idt.h"
#include "gdt.h"

struct idt_entry {
    uint16_t offset_low;    /* handler address bits 0-15   */
    uint16_t selector;      /* code segment to run it in   */
    uint8_t  zero;          /* always 0                    */
    uint8_t  type_attr;     /* present, ring, gate type    */
    uint16_t offset_high;   /* handler address bits 16-31  */
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

_Static_assert(sizeof(struct idt_entry) == 8, "IDT entry must be 8 bytes");

/* 0x8E = present (0x80) | ring 0 (0x00) | 32-bit interrupt gate (0x0E).
 * An interrupt gate also clears IF, so we can't be interrupted while
 * already handling an interrupt. */
#define IDT_INTERRUPT_GATE 0x8E

static struct idt_entry idt[256];
static struct idt_ptr   ip;

/* Defined in interrupts.asm: addresses of the 48 entry stubs */
extern uint32_t isr_stub_table[48];

void idt_set_gate(uint8_t vector, uint32_t handler, uint16_t selector, uint8_t type_attr)
{
    idt[vector].offset_low  = handler & 0xFFFF;
    idt[vector].offset_high = (handler >> 16) & 0xFFFF;
    idt[vector].selector    = selector;
    idt[vector].zero        = 0;
    idt[vector].type_attr   = type_attr;
}

void idt_init(void)
{
    ip.limit = sizeof(idt) - 1;
    ip.base  = (uint32_t)&idt;

    /* Unused vectors stay all-zero (not present). Hitting one causes a
     * General Protection Fault, which we DO handle, so nothing goes silent. */
    for (int v = 0; v < 48; v++)
        idt_set_gate(v, isr_stub_table[v], GDT_KERNEL_CODE, IDT_INTERRUPT_GATE);

    __asm__ volatile ("lidt %0" : : "m"(ip));
}
