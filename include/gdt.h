#ifndef GDT_H
#define GDT_H

#include <stdint.h>

/* Segment selectors = index * 8 (each descriptor is 8 bytes) */
#define GDT_KERNEL_CODE 0x08
#define GDT_KERNEL_DATA 0x10

void gdt_init(void);

#endif
