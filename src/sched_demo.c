#include "kernel.h"
#include "scheduler.h"

/* ================================================================
 *  DEMO ROUND ROBIN  ("schedtest")
 *
 *  Dua thread A dan B sama-sama "rakus CPU": tidak pernah yield,
 *  tidak pernah sleep. Karena itu satu-satunya hal yang membuat mereka
 *  bergantian adalah scheduler Round Robin (preempt tiap quantum habis).
 *
 *  Setiap thread mencetak SATU BARIS di awal setiap gilirannya:
 *     [waktu ms] thread X sedang jalan  (giliran N, CPU terpakai M ms)
 *  Thread berhenti sendiri setelah memakai CPU sebanyak jatahnya
 *  (budget_*), jadi jumlah giliran selalu pasti walau komputernya cepat
 *  atau lambat.
 * ================================================================ */

#define MS_PER_TICK  (1000 / TIMER_HZ)

/* Jatah CPU (pekerjaan) tiap thread TETAP, tidak tergantung quantum:
 * A selalu 180 ms dan B selalu 230 ms. Yang berubah kalau quantum diubah
 * hanyalah jumlah giliran:
 *   quantum kecil -> banyak giliran, banyak baris
 *   quantum besar -> sedikit giliran (kalau quantum >= jatah, selesai di giliran 1) */
#define BUDGET_A_TICKS  (180 / MS_PER_TICK)
#define BUDGET_B_TICKS  (230 / MS_PER_TICK)
static uint32_t budget_a = BUDGET_A_TICKS;
static uint32_t budget_b = BUDGET_B_TICKS;

#define FIB_N  25
#define SORT_N 600

static volatile uint32_t demo_running;   /* thread demo yang belum selesai */
static uint32_t demo_waiter;             /* id thread shell yang menunggu */
static uint32_t demo_start_tick;
static uint32_t demo_quantum;            /* quantum OS saat demo dimulai */
static uint32_t sort_buf[SORT_N];
static volatile uint32_t fib_sink;       /* agar hitungan Fibonacci tidak dibuang compiler */

/* ---- alat bantu ---- */

/* Cetak satu baris kalau thread ini baru mulai giliran baru.
 * Selama satu giliran thread memakai tepat 'quantum' tick, jadi nomor
 * giliran = CPU yang sudah dipakai / quantum + 1. */
static void say_turn(char letter, uint8_t color, uint32_t *last_turn) {
    uint32_t used = thread_current_cpu_ticks();
    uint32_t turn = used / demo_quantum + 1;
    if (turn == *last_turn) return;
    *last_turn = turn;

    uint32_t f = irq_save();           /* satu baris utuh, tidak terpotong preempt */
    console_set_color(color);
    kprintf("  [%6u ms] thread %c sedang jalan  (giliran %u, CPU terpakai %u ms)\n",
            (timer_ticks() - demo_start_tick) * MS_PER_TICK, letter, turn,
            used * MS_PER_TICK);
    console_set_color(COLOR_NORMAL);
    irq_restore(f);
}

static void say_done(uint8_t color, const char *what, uint32_t value) {
    uint32_t f = irq_save();
    console_set_color(color);
    kprintf("  [%6u ms] %s %u\n", (timer_ticks() - demo_start_tick) * MS_PER_TICK, what, value);
    console_set_color(COLOR_NORMAL);
    irq_restore(f);
}

static void thread_finished(void) {
    uint32_t f = irq_save();
    if (--demo_running == 0) thread_unblock(demo_waiter);   /* bangunkan shell */
    irq_restore(f);
}

static int out_of_budget(uint32_t budget_ticks) {
    return thread_current_cpu_ticks() >= budget_ticks;
}

/* ---- pekerjaan masing-masing thread ---- */

static uint32_t fib(uint32_t n) {
    return n < 2 ? n : fib(n - 1) + fib(n - 2);
}

/* A: menghitung Fibonacci ke-25 (cara rekursif yang lambat), diulang terus */
static void task_a(void) {
    uint32_t turn = 0, rounds = 0;
    while (!out_of_budget(budget_a)) {
        say_turn('A', COLOR_TITLE, &turn);
        volatile uint32_t n = FIB_N;
        fib_sink = fib(n);
        rounds++;
    }
    say_done(COLOR_TITLE, "thread A selesai: Fibonacci dihitung sebanyak", rounds);
    thread_finished();
}

/* B: bubble sort daftar 600 angka acak, diulang terus sampai jatah habis */
static void task_b(void) {
    uint32_t turn = 0, seed = 12345, lists = 0;
    while (!out_of_budget(budget_b)) {
        for (int i = 0; i < SORT_N; i++) {
            seed = seed * 1103515245u + 12345u;
            sort_buf[i] = seed >> 8;
        }
        for (int i = 0; i < SORT_N - 1; i++) {
            say_turn('B', COLOR_WARN, &turn);
            if (out_of_budget(budget_b)) break;
            for (int j = 0; j < SORT_N - 1 - i; j++) {
                if (sort_buf[j] > sort_buf[j + 1]) {
                    uint32_t t = sort_buf[j];
                    sort_buf[j] = sort_buf[j + 1];
                    sort_buf[j + 1] = t;
                }
            }
        }
        lists++;
    }
    say_done(COLOR_WARN, "thread B selesai: daftar 600 angka diurutkan sebanyak", lists);
    thread_finished();
}

/* ---- dipanggil dari perintah shell "schedtest" ----
 * Memakai quantum OS yang sedang berlaku (lihat/ubah dengan perintah "quantum").
 * Shell ditidurkan (WAITING) supaya tidak ikut mengambil giliran CPU. */
void sched_demo_run(void) {
    demo_quantum = scheduler_get_quantum();
    budget_a = BUDGET_A_TICKS;
    budget_b = BUDGET_B_TICKS;

    kprintf("Demo Round Robin: 2 thread berebut 1 CPU, tidak ada yang mau mengalah.\n");
    kprintf("  A : menghitung Fibonacci ke-%d  (selesai setelah CPU terpakai %u ms)\n", FIB_N, budget_a * MS_PER_TICK);
    kprintf("  B : mengurutkan %d angka acak  (selesai setelah CPU terpakai %u ms)\n", SORT_N, budget_b * MS_PER_TICK);
    kprintf("Quantum OS: %u tick x %u ms = %u ms (ubah dengan perintah: quantum <tick>)\n",
            demo_quantum, MS_PER_TICK, demo_quantum * MS_PER_TICK);
    kprintf("Satu baris = satu giliran CPU:\n");

    scheduler_reap("demo_A");          /* buang catatan demo sebelumnya dari "ps" */
    scheduler_reap("demo_B");

    uint32_t f = irq_save();
    demo_waiter = thread_current_id();
    demo_start_tick = timer_ticks();
    demo_running = 0;
    if (thread_create("demo_A", task_a) >= 0) demo_running++;
    if (thread_create("demo_B", task_b) >= 0) demo_running++;

    uint32_t created = demo_running;
    while (demo_running > 0)
        thread_block();                /* tidur sampai thread demo terakhir selesai */
    irq_restore(f);

    if (created == 0)
        kprintf("Gagal membuat thread demo (tabel thread penuh).\n");
    else
        kprintf("Semua thread selesai. Ketik 'ps' untuk melihat status akhirnya.\n");
}
