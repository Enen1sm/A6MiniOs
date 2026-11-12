#include "kernel.h"
#include "scheduler.h"

static volatile uint32_t ticks;

/* Program PIT channel 0 agar berdetak 'hz' kali per detik. */
void timer_init(uint32_t hz) {
    uint32_t divisor = 1193182 / hz;   /* clock dasar PIT = 1.193182 MHz */
    outb(0x43, 0x36);                  /* channel 0, mode 3, lobyte/hibyte */
    outb(0x40, divisor & 0xFF);
    outb(0x40, (divisor >> 8) & 0xFF);
}

uint32_t timer_ticks(void) { return ticks; }

/* Dipanggil dari irq0_stub setiap tick. */
void timer_irq_handler(void) {
    ticks++;
    /* EOI WAJIB dikirim SEBELUM scheduler_tick(), karena scheduler_tick()
     * bisa pindah thread dan tidak langsung kembali ke sini. */
    outb(0x20, 0x20);
    scheduler_tick();
}
