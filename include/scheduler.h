#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>

/* ================================================================
 *  PENGATURAN QUANTUM  <<<<<<  UBAH DI SINI
 *
 *  TIMER_HZ         : berapa kali timer (PIT) berdetak per detik.
 *  TIME_QUANTUM_MS  : jatah satu thread, dalam MILIDETIK.
 *
 *  OS menghitung dalam "tick" (1 tick = 1000 / TIMER_HZ ms), jadi
 *  TIME_QUANTUM_MS dikonversi otomatis ke tick (dibulatkan ke atas,
 *  minimal 1 tick). Pada 100 Hz, 1 tick = 10 ms, sehingga quantum
 *  efektif selalu kelipatan 10 ms. Untuk ketelitian 1 ms, pakai
 *  TIMER_HZ 1000.
 *
 *  Nilai ini hanya nilai AWAL. Saat OS jalan, quantum bisa diubah
 *  lewat perintah shell "quantum <ms>" (scheduler_set_quantum_ms).
 * ================================================================ */
#define TIMER_HZ            100
#define TIME_QUANTUM_MS     50      /* <<< UBAH DI SINI (milidetik) */

#define TIME_QUANTUM_TICKS  ((TIME_QUANTUM_MS * TIMER_HZ + 999) / 1000)

#define MAX_THREADS         8
#define THREAD_STACK_SIZE   8192
#define THREAD_NAME_LEN     16

typedef enum {
    THREAD_UNUSED = 0,   /* slot kosong */
    THREAD_NEW,
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_WAITING,
    THREAD_TERMINATED
} thread_state_t;

typedef struct thread {
    uint32_t       esp;               /* ESP tersimpan (dipakai switch_context) */
    uint32_t       id;
    char           name[THREAD_NAME_LEN];
    thread_state_t state;
    void         (*entry)(void);
    uint32_t       cpu_ticks;         /* total tick yang dipakai thread ini */
    uint32_t       times_scheduled;   /* berapa kali thread ini mendapat CPU */
} thread_t;

void     scheduler_init(void);
int      thread_create(const char *name, void (*entry)(void));
void     thread_exit(void);
void     thread_yield(void);
void     thread_block(void);              /* RUNNING -> WAITING (untuk mutex nanti) */
void     thread_unblock(uint32_t id);     /* WAITING -> READY */
void     scheduler_tick(void);            /* dipanggil tiap interrupt timer */
void     scheduler_set_quantum(uint32_t ticks);      /* dalam tick */
uint32_t scheduler_get_quantum(void);                /* dalam tick */
void     scheduler_set_quantum_ms(uint32_t ms);      /* dalam milidetik */
uint32_t scheduler_get_quantum_ms(void);             /* dalam milidetik */
uint32_t scheduler_total_switches(void);
void     scheduler_dump(void);            /* untuk perintah "ps" */

void     timer_init(uint32_t hz);
uint32_t timer_ticks(void);
void     timer_irq_handler(void);

void     sched_demo_start(void);          /* untuk perintah "schedtest" */

/* Assembly (switch.asm) */
extern void switch_context(uint32_t *old_esp, uint32_t new_esp);

#endif
