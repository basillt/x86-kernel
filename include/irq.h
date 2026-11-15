#ifndef IRQ_H
#define IRQ_H

#include <stdint.h>
#include "isr.h"

typedef void (*irq_handler_t)(struct registers *r);

void irq_register_handler(uint8_t irq, irq_handler_t handler);
void irq_dispatch(struct registers *r);

#endif
