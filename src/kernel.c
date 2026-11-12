#include "kernel.h"

void bootscreen_show(uint32_t magic);
void shell_run(void);

void kmain(uint32_t magic, uint32_t mbi) {
    (void)mbi;
    console_init();
    bootscreen_show(magic);
    shell_run();
}
