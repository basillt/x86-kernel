/*
 * irq.c - Routing hardware interrupts to drivers
 *
 * Drivers register a function for their IRQ number. When the IRQ
 * fires, we call it, then send End Of Interrupt to the PIC.
 */
#include "irq.h"
#include "pic.h"

static irq_handler_t handlers[16];

void irq_register_handler(uint8_t irq, irq_handler_t handler)
{
    handlers[irq] = handler;
    pic_unmask(irq);
}

void irq_dispatch(struct registers *r)
{
    uint8_t irq = r->int_no - PIC_IRQ_BASE;

    if (handlers[irq])
        handlers[irq](r);

    /* Always acknowledge, even with no handler, or the PIC stops
     * sending this IRQ and everything below it in priority. */
    pic_send_eoi(irq);
}
