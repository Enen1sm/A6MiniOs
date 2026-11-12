#include "kernel.h"
#include "scheduler.h"

static thread_t threads[MAX_THREADS];
static thread_t *current;

/* Stack tiap thread (statis, jadi tidak butuh kmalloc).
 * threads[0] = thread "main" (shell) memakai stack boot kernel. */
static uint8_t thread_stacks[MAX_THREADS][THREAD_STACK_SIZE] __attribute__((aligned(16)));

/* ---- Variabel quantum ---- */
static uint32_t time_quantum = TIME_QUANTUM_TICKS; /* jatah per thread (tick) */
static uint32_t ticks_left;                        /* sisa jatah thread yang sedang jalan */
static uint32_t total_switches;

/* ---- Fungsi inti: memilih thread berikutnya (Round Robin) ----
 * HARUS dipanggil dengan interrupt nonaktif. */
static void schedule(void) {
    thread_t *prev = current;

    if (prev->state == THREAD_RUNNING)
        prev->state = THREAD_READY;

    /* Round Robin: cari mulai dari thread SETELAH prev, melingkar. */
    thread_t *next = 0;
    for (uint32_t i = 1; i <= MAX_THREADS; i++) {
        thread_t *t = &threads[(prev->id + i) % MAX_THREADS];
        if (t->state == THREAD_READY) { next = t; break; }
    }
    if (next == 0) next = prev;      /* tidak ada yang READY lain */

    ticks_left = time_quantum;       /* <<< quantum diisi ulang setiap ganti giliran */
    next->state = THREAD_RUNNING;

    if (next == prev) return;

    next->times_scheduled++;
    total_switches++;
    current = next;
    switch_context(&prev->esp, next->esp);
}

/* ---- Dipanggil tiap tick timer (dari ISR, interrupt sudah nonaktif) ----
 * Di sinilah quantum "dihitung mundur". */
void scheduler_tick(void) {
    current->cpu_ticks++;
    if (ticks_left > 0) ticks_left--;
    if (ticks_left == 0) schedule();   /* jatah habis -> preempt */
}

/* ---- Pengaturan quantum saat runtime ---- */
void scheduler_set_quantum(uint32_t ticks) {
    if (ticks == 0) ticks = 1;
    time_quantum = ticks;              /* berlaku mulai giliran berikutnya */
}
uint32_t scheduler_get_quantum(void)    { return time_quantum; }

/* Versi milidetik: dikonversi ke tick (dibulatkan ke atas, minimal 1 tick). */
void scheduler_set_quantum_ms(uint32_t ms) {
    scheduler_set_quantum((ms * TIMER_HZ + 999) / 1000);
}
uint32_t scheduler_get_quantum_ms(void) { return time_quantum * 1000 / TIMER_HZ; }
uint32_t scheduler_total_switches(void) { return total_switches; }

/* ---- Manajemen thread ---- */
void scheduler_init(void) {
    for (uint32_t i = 0; i < MAX_THREADS; i++) {
        threads[i].state = THREAD_UNUSED;
        threads[i].id = i;
    }
    strncpy(threads[0].name, "main", THREAD_NAME_LEN);
    threads[0].state = THREAD_RUNNING;
    threads[0].times_scheduled = 1;
    current = &threads[0];
    ticks_left = time_quantum;
}

/* Dijalankan pertama kali oleh thread baru (lewat 'ret' di switch_context). */
static void thread_start(void) {
    __asm__ volatile ("sti");   /* switch terjadi di dalam ISR (IF=0), aktifkan lagi */
    current->entry();
    thread_exit();              /* fungsi thread return -> TERMINATED */
}

int thread_create(const char *name, void (*entry)(void)) {
    uint32_t flags = irq_save();
    int slot = -1;

    for (uint32_t i = 1; i < MAX_THREADS; i++) {
        if (threads[i].state == THREAD_UNUSED || threads[i].state == THREAD_TERMINATED) {
            slot = (int)i;
            break;
        }
    }
    if (slot < 0) { irq_restore(flags); return -1; }   /* tabel penuh */

    thread_t *t = &threads[slot];
    t->state = THREAD_NEW;
    t->entry = entry;
    t->cpu_ticks = 0;
    t->times_scheduled = 0;
    strncpy(t->name, name, THREAD_NAME_LEN - 1);
    t->name[THREAD_NAME_LEN - 1] = 0;

    /* Stack palsu agar switch_context bisa "memulihkan" thread yang belum
     * pernah jalan. Urutan cocok dengan pop di switch.asm:
     * edi, esi, ebx, ebp, lalu ret -> thread_start */
    uint32_t *sp = (uint32_t *)(thread_stacks[slot] + THREAD_STACK_SIZE);
    *--sp = (uint32_t)thread_start;  /* alamat return */
    *--sp = 0;                       /* ebp */
    *--sp = 0;                       /* ebx */
    *--sp = 0;                       /* esi */
    *--sp = 0;                       /* edi */
    t->esp = (uint32_t)sp;

    t->state = THREAD_READY;         /* NEW -> READY */
    irq_restore(flags);
    return slot;
}

void thread_exit(void) {
    irq_save();
    current->state = THREAD_TERMINATED;
    schedule();
    for (;;) __asm__ volatile ("hlt");   /* tidak pernah tercapai */
}

void thread_yield(void) {
    uint32_t flags = irq_save();
    schedule();
    irq_restore(flags);
}

/* Disiapkan untuk mutex/semaphore nanti. */
void thread_block(void) {
    uint32_t flags = irq_save();
    current->state = THREAD_WAITING;
    schedule();
    irq_restore(flags);
}

void thread_unblock(uint32_t id) {
    uint32_t flags = irq_save();
    if (id < MAX_THREADS && threads[id].state == THREAD_WAITING)
        threads[id].state = THREAD_READY;
    irq_restore(flags);
}

/* ---- Tampilan untuk perintah "ps" ---- */
static const char *const state_name[] = { "UNUSED", "NEW", "READY", "RUNNING", "WAITING", "TERMINATED" };

void scheduler_dump(void) {
    uint32_t flags = irq_save();   /* snapshot konsisten */
    kprintf("ID  %-10s %-11s %6s %6s\n", "NAMA", "STATE", "TICKS", "JATAH");
    for (uint32_t i = 0; i < MAX_THREADS; i++) {
        thread_t *t = &threads[i];
        if (t->state == THREAD_UNUSED) continue;
        kprintf("%-3u %-10s %-11s %6u %6u\n", t->id, t->name, state_name[t->state],
                t->cpu_ticks, t->times_scheduled);
    }
    kprintf("Context switch total: %u\n", total_switches);
    kprintf("Quantum: %u tick = %u ms (timer %u Hz)\n",
            time_quantum, time_quantum * 1000 / TIMER_HZ, (uint32_t)TIMER_HZ);
    irq_restore(flags);
}
