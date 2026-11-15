/*
 * kernel.c - The C side of the kernel. boot.asm calls kernel_main().
 */
#include <stdint.h>
#include "vga.h"

#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002

void kernel_main(uint32_t magic, uint32_t multiboot_info)
{
    vga_init();

    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_write("Hello from my kernel!\n\n");

    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_write("Multiboot magic: ");
    vga_write_hex(magic);

    if (magic == MULTIBOOT_BOOTLOADER_MAGIC) {
        vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
        vga_write("  [OK]\n");
    } else {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_write("  [BAD - not booted by Multiboot]\n");
    }

    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_write("Multiboot info at: ");
    vga_write_hex(multiboot_info);
    vga_write("\n");
}
