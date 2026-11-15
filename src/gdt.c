/*
 * gdt.c - Global Descriptor Table
 *
 * The Multiboot spec does not guarantee the state of GRUB's GDT, so
 * the kernel installs its own. We use the "flat model": every segment
 * starts at 0 and covers the full 4GB, so segmentation effectively
 * does nothing and memory protection is left to paging later.
 *
 *   index 0  selector 0x00  null descriptor (required by the CPU)
 *   index 1  selector 0x08  kernel code, ring 0
 *   index 2  selector 0x10  kernel data, ring 0
 */
#include "gdt.h"

/* One 8-byte descriptor. Intel splits base and limit into pieces
 * for historical (80286) compatibility reasons. */
struct gdt_entry {
    uint16_t limit_low;     /* limit bits 0-15                   */
    uint16_t base_low;      /* base  bits 0-15                   */
    uint8_t  base_mid;      /* base  bits 16-23                  */
    uint8_t  access;        /* present, ring, type (see below)   */
    uint8_t  granularity;   /* flags (high 4) | limit 16-19 (low 4) */
    uint8_t  base_high;     /* base  bits 24-31                  */
} __attribute__((packed));

/* The structure lgdt expects: table size - 1, then its address */
struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

_Static_assert(sizeof(struct gdt_entry) == 8, "GDT entry must be 8 bytes");
_Static_assert(sizeof(struct gdt_ptr) == 6, "GDT pointer must be 6 bytes");

/* ---- Access byte bits ---- */
#define ACC_PRESENT   0x80  /* segment is valid                         */
#define ACC_RING0     0x00  /* privilege level 0 (kernel)               */
#define ACC_CODE_DATA 0x10  /* code/data segment (not a system segment) */
#define ACC_EXEC      0x08  /* executable -> code segment               */
#define ACC_RW        0x02  /* code: readable / data: writable          */

#define ACCESS_KERNEL_CODE (ACC_PRESENT | ACC_RING0 | ACC_CODE_DATA | ACC_EXEC | ACC_RW) /* 0x9A */
#define ACCESS_KERNEL_DATA (ACC_PRESENT | ACC_RING0 | ACC_CODE_DATA | ACC_RW)            /* 0x92 */

/* ---- Flags (upper nibble of the granularity byte) ---- */
#define FLAG_GRAN_4K  0x8   /* limit is counted in 4KB pages */
#define FLAG_32BIT    0x4   /* 32-bit protected mode segment */
#define FLAGS_FLAT    (FLAG_GRAN_4K | FLAG_32BIT)                                   /* 0xC  */

#define GDT_ENTRIES 3

static struct gdt_entry gdt[GDT_ENTRIES];
static struct gdt_ptr   gp;

/* Defined in gdt_flush.asm: loads the GDT and reloads segment registers */
extern void gdt_flush(uint32_t gdt_ptr_address);

static void gdt_set_entry(int i, uint32_t base, uint32_t limit,
                          uint8_t access, uint8_t flags)
{
    gdt[i].base_low    = base & 0xFFFF;
    gdt[i].base_mid    = (base >> 16) & 0xFF;
    gdt[i].base_high   = (base >> 24) & 0xFF;

    gdt[i].limit_low   = limit & 0xFFFF;
    gdt[i].granularity = ((limit >> 16) & 0x0F) | (flags << 4);

    gdt[i].access      = access;
}

void gdt_init(void)
{
    gp.limit = sizeof(gdt) - 1;
    gp.base  = (uint32_t)&gdt;

    /* limit 0xFFFFF with 4KB granularity = 0xFFFFF * 4096 ≈ 4GB */
    gdt_set_entry(0, 0, 0,       0,                  0);           /* null */
    gdt_set_entry(1, 0, 0xFFFFF, ACCESS_KERNEL_CODE, FLAGS_FLAT);  /* code */
    gdt_set_entry(2, 0, 0xFFFFF, ACCESS_KERNEL_DATA, FLAGS_FLAT);  /* data */

    gdt_flush((uint32_t)&gp);
}
