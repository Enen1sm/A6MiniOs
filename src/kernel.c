#include "kernel.h"
#include "interrupt.h"
#include "scheduler.h"

void bootscreen_show(uint32_t magic);
void shell_run(void);

void kmain(uint32_t magic, uint32_t mbi) {
    (void)mbi;
    console_init();
    bootscreen_show(magic);

    /* ---- Interrupt + scheduler ---- */
    gdt_init();
    pic_init();
    idt_init();
    idt_set_gate(32, (uint32_t)irq0_stub);   /* IRQ0 = timer */

    scheduler_init();
    timer_init(TIMER_HZ);
    __asm__ volatile ("sti");

    shell_run();   /* shell = thread "main", ikut di-preempt */
}
