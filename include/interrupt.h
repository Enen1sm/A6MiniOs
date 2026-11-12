#ifndef INTERRUPT_H
#define INTERRUPT_H

#include <stdint.h>

void gdt_init(void);
void pic_init(void);   /* remap IRQ ke vektor 32..47, semua IRQ di-mask kecuali IRQ0 */
void idt_init(void);
void idt_set_gate(uint8_t num, uint32_t handler);

/* Assembly (isr.asm) */
extern void irq0_stub(void);
extern void isr_default(void);

#endif
