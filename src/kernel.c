/*
 * kernel.c - The C side of the kernel. boot.asm calls kernel_main().
 */
#include <stdint.h>
#include "vga.h"
#include "gdt.h"
#include "idt.h"
#include "pic.h"
#include "keyboard.h"
#include "pci.h"
#include "nvme.h"

#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002
#define MAX_PCI_DEVICES 32

/* Boot log line, Linux style: "[ OK ] message" or "[FAIL] message" */
static void status(int ok, const char *msg)
{
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_write("[");
    vga_set_color(ok ? VGA_LIGHT_GREEN : VGA_LIGHT_RED, VGA_BLACK);
    vga_write(ok ? " OK " : "FAIL");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_write("] ");
    vga_write(msg);
}

/* Ask the CPU which GDT and segments it's really using */
static void check_gdt(void)
{
    struct __attribute__((packed)) { uint16_t limit; uint32_t base; } gdtr;
    uint16_t cs, ds, ss;

    __asm__ volatile ("sgdt %0" : "=m"(gdtr));
    __asm__ volatile ("mov %%cs, %0" : "=r"(cs));
    __asm__ volatile ("mov %%ds, %0" : "=r"(ds));
    __asm__ volatile ("mov %%ss, %0" : "=r"(ss));

    int ok = cs == GDT_KERNEL_CODE && ds == GDT_KERNEL_DATA && ss == GDT_KERNEL_DATA;
    status(ok, "GDT loaded: base ");
    vga_write_hex(gdtr.base);
    vga_write(", CS="); vga_write_hex(cs);
    vga_write(" DS=");  vga_write_hex(ds);
    vga_write(" SS=");  vga_write_hex(ss);
    vga_write("\n");
}

void kernel_main(uint32_t magic, uint32_t multiboot_info)
{
    (void)multiboot_info;
    vga_init();

    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_write("Hello from my kernel!\n\n");

    /* ---- Part 1: did the bootloader hand over control properly? ---- */
    status(magic == MULTIBOOT_BOOTLOADER_MAGIC, "Multiboot magic ");
    vga_write_hex(magic);
    vga_write("\n");

    /* ---- Part 2: our own segments ---- */
    gdt_init();
    check_gdt();

    /* ---- Part 3: exception handlers ---- */
    idt_init();
    status(1, "IDT loaded: 32 exception handlers, 16 IRQ handlers\n");

    /* ---- Part 4: hardware interrupts + keyboard ---- */
    pic_init();
    status(1, "PIC remapped: IRQ 0-15 -> vectors 32-47\n");
    keyboard_init();
    status(1, "Keyboard driver on IRQ 1\n");

    /* ---- Part 5: what's on the PCI bus? ---- */
    struct pci_device devices[MAX_PCI_DEVICES];
    vga_write("\nPCI devices:\n");
    int n = pci_scan(devices, MAX_PCI_DEVICES);
    if (n > MAX_PCI_DEVICES)
        n = MAX_PCI_DEVICES;

    int nvme_found = 0;
    for (int i = 0; i < n; i++) {
        /* class 01 (storage), subclass 08 (NVM), prog-if 02 (NVMe) */
        if (devices[i].class_code == 0x01 && devices[i].subclass == 0x08 &&
            devices[i].prog_if == 0x02) {
            if (!nvme_found)
                vga_write("\n");
            nvme_probe(&devices[i]);
            nvme_found = 1;
        }
    }

    __asm__ volatile ("sti");   /* interrupts on: from now on the CPU can be interrupted */

    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_write("\nType something! (F1 = divide by zero, to test the exception handler)\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_write("> ");

    /* Sleep until the next interrupt, forever. */
    for (;;)
        __asm__ volatile ("hlt");
}
