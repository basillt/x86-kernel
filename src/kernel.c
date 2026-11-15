/*
 * kernel.c - The C side of the kernel. boot.asm calls kernel_main().
 */
#include <stdint.h>
#include "vga.h"
#include "gdt.h"
#include "idt.h"

#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002

static void print_ok(void)
{
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_write("  [OK]\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
}

static void print_fail(const char *why)
{
    vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
    vga_write("  [FAIL] ");
    vga_write(why);
    vga_write("\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
}

/* Read back what the CPU is actually using, to prove the GDT took effect */
static void show_gdt_state(void)
{
    struct __attribute__((packed)) { uint16_t limit; uint32_t base; } gdtr;
    uint16_t cs, ds, ss;

    __asm__ volatile ("sgdt %0" : "=m"(gdtr));
    __asm__ volatile ("mov %%cs, %0" : "=r"(cs));
    __asm__ volatile ("mov %%ds, %0" : "=r"(ds));
    __asm__ volatile ("mov %%ss, %0" : "=r"(ss));

    vga_write("  GDTR base  = "); vga_write_hex(gdtr.base);  vga_write("\n");
    vga_write("  GDTR limit = "); vga_write_hex(gdtr.limit); vga_write("  (3 entries * 8 - 1 = 23)\n");
    vga_write("  CS = "); vga_write_hex(cs);
    vga_write("   DS = "); vga_write_hex(ds);
    vga_write("   SS = "); vga_write_hex(ss);

    if (cs == GDT_KERNEL_CODE && ds == GDT_KERNEL_DATA && ss == GDT_KERNEL_DATA)
        print_ok();
    else
        print_fail("segment registers not reloaded");
}

void kernel_main(uint32_t magic, uint32_t multiboot_info)
{
    vga_init();

    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_write("Hello from my kernel!\n\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);

    /* ---- Section 1: Multiboot ---- */
    vga_write("Multiboot magic: ");
    vga_write_hex(magic);
    if (magic == MULTIBOOT_BOOTLOADER_MAGIC)
        print_ok();
    else
        print_fail("not booted by Multiboot");

    vga_write("Multiboot info at: ");
    vga_write_hex(multiboot_info);
    vga_write("\n\n");

    /* ---- Section 2: GDT ---- */
    vga_write("Loading GDT...\n");
    gdt_init();
    show_gdt_state();

    /* ---- Section 3: IDT ---- */
    vga_write("\nLoading IDT...");
    idt_init();
    print_ok();

    /* Prove the exception handler works: divide by zero on purpose.
     * volatile stops the compiler from noticing and removing it. */
    vga_write("Dividing by zero on purpose...\n");
    volatile int a = 1, b = 0;
    volatile int c = a / b;
    (void)c;

    vga_write("If you can read this, the exception was ignored!\n");
}
