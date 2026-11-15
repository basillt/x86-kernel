/*
 * pci.c - Finding devices on the PCI bus
 *
 * Every PCI device has a 256-byte "configuration space" describing it:
 * who made it (vendor ID), what it is (class code), and where its
 * registers live (BARs). We reach it through two I/O ports:
 *
 *   0xCF8  CONFIG_ADDRESS  - write which bus/slot/function/offset you want
 *   0xCFC  CONFIG_DATA     - then read (or write) 32 bits from there
 *
 * Addresses: up to 256 buses, 32 slots per bus, 8 functions per slot.
 */
#include "pci.h"
#include "io.h"
#include "vga.h"

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

static uint32_t pci_address(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset)
{
    return (1u << 31)                   /* enable bit                      */
         | ((uint32_t)bus  << 16)
         | ((uint32_t)slot << 11)
         | ((uint32_t)func << 8)
         | (offset & 0xFC);             /* must be 4-byte aligned          */
}

uint32_t pci_config_read(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset)
{
    outl(PCI_CONFIG_ADDRESS, pci_address(bus, slot, func, offset));
    return inl(PCI_CONFIG_DATA);
}

void pci_config_write(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value)
{
    outl(PCI_CONFIG_ADDRESS, pci_address(bus, slot, func, offset));
    outl(PCI_CONFIG_DATA, value);
}

/* ---- Human-readable names (just the common ones) ---- */

static const char *vendor_name(uint16_t vendor)
{
    switch (vendor) {
    case 0x8086: return "Intel";
    case 0x1022: return "AMD";
    case 0x10DE: return "NVIDIA";
    case 0x1234: return "QEMU/Bochs";
    case 0x1B36: return "Red Hat (QEMU)";
    case 0x1AF4: return "Red Hat (virtio)";
    case 0x10EC: return "Realtek";
    case 0x15B7: return "Sandisk";
    case 0x144D: return "Samsung";
    default:     return "Unknown vendor";
    }
}

static const char *class_name(uint8_t cls, uint8_t sub, uint8_t prog_if)
{
    switch (cls) {
    case 0x01:
        switch (sub) {
        case 0x01: return "IDE controller";
        case 0x06: return "SATA controller";
        case 0x08: return prog_if == 0x02 ? "NVMe controller" : "Non-volatile memory controller";
        default:   return "Mass storage controller";
        }
    case 0x02: return sub == 0x00 ? "Ethernet controller" : "Network controller";
    case 0x03: return "VGA display controller";
    case 0x04: return "Multimedia controller";
    case 0x06:
        switch (sub) {
        case 0x00: return "Host bridge";
        case 0x01: return "ISA bridge";
        case 0x04: return "PCI-to-PCI bridge";
        case 0x80: return "Bridge (other)";
        default:   return "Bridge";
        }
    case 0x0C: return sub == 0x03 ? "USB controller" : "Serial bus controller";
    default:   return "Other device";
    }
}

/* ---- Printing helpers ---- */

static void print_hex_digits(uint32_t value, int digits)
{
    const char *hex = "0123456789ABCDEF";
    for (int shift = (digits - 1) * 4; shift >= 0; shift -= 4)
        vga_putchar(hex[(value >> shift) & 0xF]);
}

static void print_device(const struct pci_device *d)
{
    /* Linux-style location: bus:slot.function */
    vga_write("  ");
    print_hex_digits(d->bus, 2);  vga_putchar(':');
    print_hex_digits(d->slot, 2); vga_putchar('.');
    print_hex_digits(d->func, 1);
    vga_write("  ");
    print_hex_digits(d->vendor_id, 4); vga_putchar(':');
    print_hex_digits(d->device_id, 4);
    vga_write("  ");
    vga_write(class_name(d->class_code, d->subclass, d->prog_if));
    vga_write(" (");
    vga_write(vendor_name(d->vendor_id));
    vga_write(")\n");
}

/* ---- Scanning ---- */

static int check_function(uint8_t bus, uint8_t slot, uint8_t func,
                          struct pci_device *out, int count, int max)
{
    uint32_t id = pci_config_read(bus, slot, func, 0x00);
    uint16_t vendor = id & 0xFFFF;
    if (vendor == 0xFFFF)               /* nothing here: reads return all 1s */
        return count;

    uint32_t class_reg = pci_config_read(bus, slot, func, 0x08);

    struct pci_device d = {
        .bus = bus, .slot = slot, .func = func,
        .vendor_id  = vendor,
        .device_id  = id >> 16,
        .class_code = class_reg >> 24,
        .subclass   = (class_reg >> 16) & 0xFF,
        .prog_if    = (class_reg >> 8) & 0xFF,
    };

    print_device(&d);
    if (count < max)
        out[count] = d;
    return count + 1;
}

int pci_scan(struct pci_device *out, int max)
{
    int count = 0;

    /* Brute force: try every bus and slot. Simple, and fast enough. */
    for (int bus = 0; bus < 256; bus++) {
        for (int slot = 0; slot < 32; slot++) {
            uint32_t id = pci_config_read(bus, slot, 0, 0x00);
            if ((id & 0xFFFF) == 0xFFFF)
                continue;

            count = check_function(bus, slot, 0, out, count, max);

            /* Header type bit 7 = multi-function device: check functions 1-7 */
            uint8_t header_type = (pci_config_read(bus, slot, 0, 0x0C) >> 16) & 0xFF;
            if (header_type & 0x80)
                for (int func = 1; func < 8; func++)
                    count = check_function(bus, slot, func, out, count, max);
        }
    }
    return count;
}
