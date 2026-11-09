#include "kernel.h"

void bootscreen_show(uint32_t magic);

void kmain(uint32_t magic, uint32_t mbi) {
    (void)mbi;
    console_init();
    bootscreen_show(magic);

    for (;;) __asm__ volatile("hlt");
}