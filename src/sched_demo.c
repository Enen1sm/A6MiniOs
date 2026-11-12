#include "kernel.h"
#include "scheduler.h"

/* Perbesar/kecilkan agar jumlah huruf per quantum pas di layar. */
#define DELAY_LOOPS 2000000

static void delay(void) {
    for (volatile uint32_t i = 0; i < DELAY_LOOPS; i++) { }
}

static void worker(char letter, int count) {
    for (int i = 0; i < count; i++) {
        kprintf("%c", letter);
        delay();
    }
}

static void task_a(void) { worker('A', 80);  kprintf("\n[task_A selesai]\n"); }
static void task_b(void) { worker('B', 120); kprintf("\n[task_B selesai]\n"); }

/* Dipanggil dari perintah shell "schedtest". Dua thread dengan tugas berbeda;
 * keduanya berakhir sendiri sehingga status TERMINATED bisa dilihat di "ps". */
void sched_demo_start(void) {
    thread_create("task_A", task_a);
    thread_create("task_B", task_b);
}
