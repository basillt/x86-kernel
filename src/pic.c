/*
 * pic.c - The 8259 Programmable Interrupt Controller
 *
 * Hardware devices don't interrupt the CPU directly. They raise a line
 * (IRQ 0-15) on the PIC, which forwards it to the CPU as an interrupt
 * vector. There are two PICs chained together: the master handles
 * IRQ 0-7, the slave handles IRQ 8-15 and is wired into master IRQ 2.
 *
 * Problem: by default the master sends IRQ 0-7 as vectors 8-15, which
 * collide with CPU exceptions (vector 8 is "double fault"!). So the
 * first job is to remap them to vectors 32-47.
 */
#include "pic.h"
#include "io.h"

#define PIC1_CMD   0x20     /* master */
#define PIC1_DATA  0x21
#define PIC2_CMD   0xA0     /* slave  */
#define PIC2_DATA  0xA1

#define PIC_EOI    0x20     /* "End Of Interrupt" command */

#define ICW1_INIT  0x10     /* start initialization...       */
#define ICW1_ICW4  0x01     /* ...and we'll send ICW4 too    */
#define ICW4_8086  0x01     /* 8086 mode (not old 8080 mode) */

void pic_init(void)
{
    /* The PIC is set up with a fixed sequence of 4 "Initialization
     * Command Words" (ICW1..ICW4), sent to both chips. */
    outb(PIC1_CMD, ICW1_INIT | ICW1_ICW4); io_wait();
    outb(PIC2_CMD, ICW1_INIT | ICW1_ICW4); io_wait();

    outb(PIC1_DATA, PIC_IRQ_BASE);     io_wait();   /* ICW2: master -> vectors 32-39 */
    outb(PIC2_DATA, PIC_IRQ_BASE + 8); io_wait();   /* ICW2: slave  -> vectors 40-47 */

    outb(PIC1_DATA, 0x04); io_wait();   /* ICW3: master has a slave on IRQ 2 (bit 2) */
    outb(PIC2_DATA, 0x02); io_wait();   /* ICW3: slave's cascade identity is 2       */

    outb(PIC1_DATA, ICW4_8086); io_wait();
    outb(PIC2_DATA, ICW4_8086); io_wait();

    /* Mask (disable) every IRQ. Drivers unmask the ones they need.
     * IRQ 2 stays enabled on the master so slave IRQs can get through. */
    outb(PIC1_DATA, 0xFF & ~(1 << 2));
    outb(PIC2_DATA, 0xFF);
}

/* Tell the PIC we're done with this IRQ. Until we do, it won't send
 * this IRQ (or any lower-priority one) again. */
void pic_send_eoi(uint8_t irq)
{
    if (irq >= 8)
        outb(PIC2_CMD, PIC_EOI);    /* slave IRQ: both chips need to know */
    outb(PIC1_CMD, PIC_EOI);
}

void pic_unmask(uint8_t irq)
{
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    uint8_t  bit  = irq % 8;
    outb(port, inb(port) & ~(1 << bit));
}
