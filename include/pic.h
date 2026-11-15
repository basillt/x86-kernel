#ifndef PIC_H
#define PIC_H

#include <stdint.h>

#define PIC_IRQ_BASE 32     /* IRQ 0 -> vector 32, ..., IRQ 15 -> vector 47 */

void pic_init(void);
void pic_send_eoi(uint8_t irq);
void pic_unmask(uint8_t irq);

#endif
