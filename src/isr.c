/*
 * isr.c - What happens after interrupts.asm hands control to C.
 *
 * Vectors 0-31 are CPU exceptions: something went wrong in the code
 * that was running. For now there's no way to recover, so we show a
 * panic screen with everything useful for debugging, and halt.
 */
#include "isr.h"
#include "vga.h"

static const char *exception_names[32] = {
    "Divide by zero",               "Debug",
    "Non-maskable interrupt",       "Breakpoint",
    "Overflow",                     "Bound range exceeded",
    "Invalid opcode",               "Device not available",
    "Double fault",                 "Coprocessor segment overrun",
    "Invalid TSS",                  "Segment not present",
    "Stack-segment fault",          "General protection fault",
    "Page fault",                   "Reserved",
    "x87 floating-point exception", "Alignment check",
    "Machine check",                "SIMD floating-point exception",
    "Virtualization exception",     "Control protection exception",
    "Reserved",                     "Reserved",
    "Reserved",                     "Reserved",
    "Reserved",                     "Reserved",
    "Hypervisor injection",         "VMM communication exception",
    "Security exception",           "Reserved",
};

static void print_reg(const char *name, uint32_t value)
{
    vga_write(name);
    vga_write_hex(value);
    vga_write("   ");
}

/* Draw the panic message as a red block under whatever was already on
 * screen, so the boot log stays visible above it. */
static void kernel_panic(struct registers *r)
{
    vga_write("\n");
    vga_set_color(VGA_WHITE, VGA_RED);

    vga_pad_line();
    vga_write("  KERNEL PANIC: ");
    vga_write(exception_names[r->int_no]);
    vga_write("  (exception ");
    vga_write_hex(r->int_no);
    vga_write(", error code ");
    vga_write_hex(r->err_code);
    vga_write(")");
    vga_pad_line();
    vga_pad_line();

    vga_write("  "); print_reg("EIP=", r->eip); print_reg("CS=", r->cs);
    print_reg("EFLAGS=", r->eflags); vga_pad_line();
    vga_write("  "); print_reg("EAX=", r->eax); print_reg("EBX=", r->ebx);
    print_reg("ECX=", r->ecx); vga_pad_line();
    vga_write("  "); print_reg("EDX=", r->edx); print_reg("ESI=", r->esi);
    print_reg("EDI=", r->edi); vga_pad_line();
    vga_write("  "); print_reg("EBP=", r->ebp); print_reg("ESP=", r->esp);
    vga_pad_line();
    vga_pad_line();
    vga_write("  The system has been halted.");
    vga_pad_line();
    vga_pad_line();

    for (;;)
        __asm__ volatile ("cli; hlt");
}

void interrupt_dispatch(struct registers *r)
{
    if (r->int_no < 32)
        kernel_panic(r);
}
