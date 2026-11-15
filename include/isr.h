#ifndef ISR_H
#define ISR_H

#include <stdint.h>

/* Everything interrupts.asm pushes, in memory order (lowest address first).
 * The C handler receives a pointer to this. */
struct registers {
    uint32_t gs, fs, es, ds;                          /* pushed by our stub */
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;  /* pusha              */
    uint32_t int_no, err_code;                        /* pushed by our stub */
    uint32_t eip, cs, eflags;                         /* pushed by the CPU  */
};

void interrupt_dispatch(struct registers *r);

#endif
