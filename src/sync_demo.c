#include "kernel.h"
#include "scheduler.h"
#include "sync.h"

#define COUNT       100        /* tiap thread menambah counter sebanyak ini */
#define DELAY_LOOPS 2000000    /* sama seperti sched_demo.c (kira-kira 7 ms) */

static volatile uint32_t counter;      /* data yang dipakai bersama */
static mutex_t lock;
static int use_mutex;
static int started;                    /* sudah pernah dijalankan? */
static int n_threads;                  /* jumlah thread pekerja */
static volatile int finished;          /* berapa thread yang sudah selesai */

static void delay(void) {
    for (volatile uint32_t i = 0; i < DELAY_LOOPS; i++) { }
}

static void worker(void) {
    for (int i = 0; i < COUNT; i++) {
        if (use_mutex) mutex_lock(&lock);

        uint32_t temp = counter;       /* 1. baca nilai counter       */
        delay();                       /*    (di sini thread bisa diganti) */
        counter = temp + 1;            /* 2. tulis nilai + 1          */

        if (use_mutex) mutex_unlock(&lock);
        delay();
    }
    uint32_t f = irq_save();
    finished++;
    irq_restore(f);
}

static void start(int with_mutex) {
    if (started && finished < n_threads) {
        kprintf("Demo sebelumnya belum selesai. Ketik: race status\n");
        return;
    }
    counter = 0;
    finished = 0;
    n_threads = 0;
    use_mutex = with_mutex;
    mutex_init(&lock);

    if (thread_create("race_A", worker) >= 0) n_threads++;
    if (thread_create("race_B", worker) >= 0) n_threads++;
    started = 1;

    kprintf("Mulai: %d thread, masing-masing menambah counter %d kali. %s\n",
            n_threads, COUNT, with_mutex ? "(PAKAI mutex)" : "(TANPA mutex)");
    kprintf("Counter seharusnya jadi %d. Tunggu sebentar, lalu ketik: race status\n",
            n_threads * COUNT);
}

static void status(void) {
    if (!started) { kprintf("Belum ada demo. Ketik: race nolock atau race lock\n"); return; }
    int expected = n_threads * COUNT;

    if (finished < n_threads) {
        kprintf("Masih berjalan... counter sementara = %u\n", counter);
        return;
    }
    kprintf("Mode          : %s\n", use_mutex ? "PAKAI mutex" : "TANPA mutex");
    kprintf("Counter akhir : %u\n", counter);
    kprintf("Seharusnya    : %d\n", expected);

    if ((int)counter == expected)
        kprintf("Hasil: BENAR. %s\n", use_mutex ? "Mutex berhasil melindungi data." : "(kebetulan, coba ulang)");
    else
        kprintf("Hasil: SALAH, hilang %d. Ini yang disebut race condition.\n", expected - (int)counter);
}

void race_command(int argc, char **argv) {
    const char *sub = (argc >= 2) ? argv[1] : "";
    if      (!strcmp(sub, "nolock")) start(0);
    else if (!strcmp(sub, "lock"))   start(1);
    else if (!strcmp(sub, "status")) status();
    else {
        kprintf("pemakaian:\n");
        kprintf("   race nolock    tanpa mutex (hasil salah)\n");
        kprintf("   race lock      pakai mutex (hasil benar)\n");
        kprintf("   race status    lihat hasil\n");
    }
}
