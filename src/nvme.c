/*
 * nvme.c - First contact with an NVMe controller
 *
 * An NVMe SSD is a PCI device. Its control registers are memory-mapped:
 * BAR0/BAR1 in its PCI configuration space hold the physical address
 * where they appear. Paging is still off in this kernel, so that address
 * can be read with a plain pointer.
 *
 * This doesn't send any commands yet. It just reads the registers that
 * describe the controller, which is the first thing a real driver (or a
 * validation engineer at bring-up) checks.
 *
 * Register offsets are from the NVMe Base Specification:
 *   0x00 CAP   Controller Capabilities (64-bit)
 *   0x08 VS    Version
 *   0x14 CC    Controller Configuration
 *   0x1C CSTS  Controller Status
 */
#include "nvme.h"
#include "vga.h"

#define PCI_COMMAND         0x04
#define PCI_BAR0            0x10
#define PCI_BAR1            0x14
#define PCI_CMD_MEMORY      (1 << 1)   /* respond to memory accesses (MMIO) */

#define NVME_REG_CAP_LO     (0x00 / 4)
#define NVME_REG_CAP_HI     (0x04 / 4)
#define NVME_REG_VS         (0x08 / 4)
#define NVME_REG_CC         (0x14 / 4)
#define NVME_REG_CSTS       (0x1C / 4)

static void print_location(const struct pci_device *d)
{
    const char *hex = "0123456789ABCDEF";
    vga_putchar(hex[d->bus >> 4]);  vga_putchar(hex[d->bus & 0xF]);  vga_putchar(':');
    vga_putchar(hex[d->slot >> 4]); vga_putchar(hex[d->slot & 0xF]); vga_putchar('.');
    vga_putchar(hex[d->func & 0xF]);
}

void nvme_probe(const struct pci_device *d)
{
    uint32_t bar0 = pci_config_read(d->bus, d->slot, d->func, PCI_BAR0);
    uint32_t bar1 = pci_config_read(d->bus, d->slot, d->func, PCI_BAR1);

    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_write("NVMe controller at ");
    print_location(d);
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);

    /* BAR bit 0: 0 = memory, 1 = I/O ports. Bits 2:1 = 0b10 means 64-bit. */
    if (bar0 & 1) {
        vga_write(": BAR0 is I/O space, expected memory\n");
        return;
    }
    int is_64bit = ((bar0 >> 1) & 0x3) == 0x2;
    if (is_64bit && bar1 != 0) {
        vga_write(": registers above 4GB, unreachable in 32-bit mode\n");
        return;
    }
    uint32_t base = bar0 & ~0xFu;

    vga_write(", registers at ");
    vga_write_hex(base);
    vga_write("\n");

    /* Make sure the device answers memory accesses. Write back only the
     * command half: the status half has bits that clear when written as 1. */
    uint32_t cmd = pci_config_read(d->bus, d->slot, d->func, PCI_COMMAND);
    pci_config_write(d->bus, d->slot, d->func, PCI_COMMAND, (cmd & 0xFFFF) | PCI_CMD_MEMORY);

    volatile uint32_t *regs = (volatile uint32_t *)base;
    uint32_t cap_lo = regs[NVME_REG_CAP_LO];
    uint32_t cap_hi = regs[NVME_REG_CAP_HI];
    uint32_t vs     = regs[NVME_REG_VS];
    uint32_t cc     = regs[NVME_REG_CC];
    uint32_t csts   = regs[NVME_REG_CSTS];

    uint32_t max_queue   = (cap_lo & 0xFFFF) + 1;          /* CAP.MQES is 0-based   */
    uint32_t timeout_ms  = ((cap_lo >> 24) & 0xFF) * 500;  /* CAP.TO, 500 ms units  */
    uint32_t db_stride   = 4u << (cap_hi & 0xF);           /* CAP.DSTRD             */
    uint32_t min_page_kb = 4u << ((cap_hi >> 16) & 0xF);   /* CAP.MPSMIN            */

    vga_write("  Version ");
    vga_write_dec(vs >> 16);          vga_putchar('.');
    vga_write_dec((vs >> 8) & 0xFF);  vga_putchar('.');
    vga_write_dec(vs & 0xFF);
    vga_write(" | max queue ");       vga_write_dec(max_queue);
    vga_write(" | doorbell stride "); vga_write_dec(db_stride);
    vga_write("B\n");

    vga_write("  Ready timeout ");    vga_write_dec(timeout_ms);
    vga_write(" ms | min page ");     vga_write_dec(min_page_kb);
    vga_write("KB | enabled=");       vga_write_dec(cc & 1);
    vga_write(" ready=");             vga_write_dec(csts & 1);
    if (csts & 2)
        vga_write(" FATAL");
    vga_write("\n");
}
