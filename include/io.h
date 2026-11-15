#ifndef IO_H
#define IO_H

#include <stdint.h>

/*
 * x86 has a separate "I/O port" address space (0x0000-0xFFFF) used to
 * talk to devices like the PIC, the keyboard controller and the VGA
 * cursor. You can't reach it with normal pointers, only with the
 * in/out instructions.
 */

static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

/* 32-bit versions, needed for PCI configuration space */
static inline void outl(uint16_t port, uint32_t value)
{
    __asm__ volatile ("outl %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint32_t inl(uint16_t port)
{
    uint32_t value;
    __asm__ volatile ("inl %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

/* Writing to unused port 0x80 takes ~1 microsecond: a tiny delay
 * for old hardware (like the PIC) that needs time between commands. */
static inline void io_wait(void)
{
    outb(0x80, 0);
}

#endif
