#include "kernel.h"
#include "scheduler.h"
#include "deadlock.h"

//MUTEX

#define MAX_WAIT 8

typedef struct {
    bool        used;
    bool        waiting;
    bool        aborted;
    uint32_t    tid;
    dl_mutex_t  *m;
    uint32_t    seq;
} wait_t;

static wait_t   waits[MAX_WAIT];
static uint32_t seq_counter;

void dl_mutex_init(dl_mutex_t *m, const char *name, int rank) {
    m->name  = name;
    m->rank  = rank;
    m->owner = -1;
}

static wait_t *find_wait_of(uint32_t tid) {
    for (int i = 0; i < MAX_WAIT; i++) {
        if (waits[i].used && waits[i].waiting && waits[i].tid == tid) return &waits[i];
    }
    return NULL;
}

int dl_count_waiting(void) {
    uint32_t f = irq_save();
    int n = 0;
    for (int i = 0; i < MAX_WAIT; i++)
        if (waits[i].used && waits[i].waiting) n++;
    irq_restore(f);
    return n;
}

int dl_lock(dl_mutex_t *m) {
    uint32_t f  = irq_save();
    uint32_t me = thread_current_id();

    if (m->owner == (int)me) {
        kprintf("   [dl] mutex %s sudah dipegang thread ini (self-deadlock dicegah)\n", m->name);
        irq_restore(f);
        return 0;
    }
    if (m->owner < 0) {
        m->owner = (int)me;
        irq_restore(f);
        return 0;
    }

    wait_t *w = NULL;
    for (int i = 0; i < MAX_WAIT && !w; i++) if (!waits[i].used) w = &waits[i];
    if (!w) {
        kprintf("   [dl] tabel tunggu penuh, permintaan mutex %s ditolak\n", m->name);
        irq_restore(f);
        return -1;
    }

    w->used = true; w->waiting = true; w->aborted = false;
    w->tid = me;    w->m = m;          w->seq = ++seq_counter;

    thread_block();

    int r = w->aborted ? -1 : 0;
    w->used = false;
    irq_restore(f);
    return r;
}

void dl_unlock(dl_mutex_t *m) {
    uint32_t f = irq_save();
    if (m->owner != (int)thread_current_id()) {
        kprintf("   [dl] unlcok %s ditolak: bukan pemilik\n", m->name);
        irq_restore(f);
        return;
    }

    wait_t *best = NULL;
    for (int i = 0; i < MAX_WAIT; i++) {
        wait_t *w = &waits[i];
        if (w->used && w->waiting && w->m == m && (!best || w->seq < best->seq)) best = w;
    }
    if (best) {
        best->waiting = false;
        m->owner = (int)best->tid;
        thread_unblock(best->tid);
    } else {
        m->owner = -1;
    }
    irq_restore(f);
}

// DETEKSI DAN PEMULIHAN
bool dl_find_cycle(uint32_t *victim) {
    uint32_t f = irq_save();
    bool found = false;

    for (int s = 0; s < MAX_WAIT && !found; s++) {
        wait_t *start = &waits[s];
        if (!start->used || !start->waiting) continue;

        uint32_t path[MAX_WAIT];
        int n = 0;
        wait_t *cur = start;while (cur && n < MAX_WAIT) {
            path[n++] = cur->tid;
            int own = cur->m->owner;
            if (own < 0) break;
            if ((uint32_t)own == start->tid) { found = true; break; }
            cur = find_wait_of((uint32_t)own);
        }

        if (found) {
            uint32_t worst = path[0];
            for (int i = 0; i < n; i++) {
                wait_t *w = find_wait_of(path[i]);
                kprintf("   T%u MENUNGGU mutex %s (dipegang T%u)\n", path[i], w->m->name, (uint32_t)w->m->owner);
                if (path[i] > worst) worst = path[i];
            }
            kprintf("   -> kembali ke T%u: LINGKARAN TERTUTUP (circular wait)\n", start->tid);
            if (victim) *victim = worst;
        }
    }
    irq_restore(f);
    return found;
}

void dl_abort(uint32_t victim) {
    uint32_t f = irq_save();
    wait_t *w = find_wait_of(victim);
    if (w) {
        w->waiting = false;
        w->aborted = true;
        thread_unblock(victim);
    }
    irq_restore(f);
}

// DEMO
typedef struct {
    const char *tag;
    dl_mutex_t *first;
    dl_mutex_t *second;
} job_t;

static dl_mutex_t   mtx_a, mtx_b;
static job_t        jobs[2];
static bool         fixed_mode;
static volatile int holding[2];
static volatile int finished[2];

static void run_job(int idx) {
    job_t *j = &jobs[idx];
    kprintf("   [%s] mengambil mutex %s\n", j->tag, j->first->name);
    dl_lock(j->first);
    holding[idx] = 1;

    if (!fixed_mode) {
        while (!holding[1 - idx]) thread_yield();
    } else {
        for (int i = 0; i < 20; i++) thread_yield();
    }

    kprintf("   [%s] memegang %s, mencoba mengambil mutex %s ...\n", j->tag, j->first->name, j->second->name);
    if (dl_lock(j->second) < 0) {
        kprintf("   [%s] DIBATALKAN (dipilih jadi korban), melepas mutex %s\n", j->tag, j->first->name);
        dl_unlock(j->first);
        finished[idx] = 1;
        return;
    }
    kprintf("   [%s] berhasil memegang %s dan %s -> bekerja\n", j->tag, j->first->name, j->second->name);
    for (int i = 0; i < 10; i++) thread_yield();
    dl_unlock(j->second);
    dl_unlock(j->first);
    kprintf("   [%s] selesai, semua mutex dilepas\n", j->tag);
    finished[idx] = 1;
}

static void entry_satu(void) { run_job(0); }
static void entry_dua(void)  { run_job(1); }

static void setup_jobs(bool ordered) {
    dl_mutex_init(&mtx_a, "A", 1);
    dl_mutex_init(&mtx_b, "B", 2);
    jobs[0] = (job_t){ "satu", &mtx_a, &mtx_b };
    jobs[1] = (job_t){ "dua",  &mtx_b, &mtx_a };
    holding[0] = holding[1] = 0;
    finished[0] = finished [1] = 0;
    fixed_mode = ordered;

    if (ordered) {
        for (int i = 0; i < 2; i++) {
            if (jobs[i].first->rank > jobs[i].second->rank) {
                kprintf("   [%s] diminta B lalu A -> diurutkan jadi A lalu B (rank)\n", jobs[i].tag);
                dl_mutex_t *tmp = jobs[i].first;
                jobs[i].first = jobs[i].second;
                jobs[i].second = tmp;
            }
        }
    }
}

#define GUARD 300000

static void run_demo(bool fixed) {
    console_set_color(COLOR_TITLE);
    kprintf(fixed ? "=== DEMO PENCEGAHAN DEADLOCK: LOCK ORDERING ===\n" : "=== DEMO DEADLOCK: DUA THREAD, DUA MUTEX, URUTAN BERLAWANAN ===\n");
    console_set_color(COLOR_NORMAL);

    if (!fixed) {
        kprintf("Empat syarat deadlock (Coffman): mutual exclusion, hold and wait,\n");
        kprintf("no preemption, circular wait. Demo ini menciptakan keempatnya.\n\n");
    }

    setup_jobs(fixed);

    uint32_t fl = irq_save();
    int t1 = thread_create("satu", entry_satu);
    int t2 = thread_create("dua", entry_dua);
    irq_restore(fl);
    if (t1 < 0 || t2 < 0) { kprintf("gagal membuat thread (tabel penuh)\n"); return; }
    kprintf("   thread satu = T%d, thread dua = T%d\n", t1, t2);

    if (!fixed) {
        for (uint32_t g = 0; dl_count_waiting() < 2 && g < GUARD; g++) thread_yield();

        kprintf("\n-- Status thread saat macet (keduanya WAITING) --\n");
        scheduler_dump();

        kprintf("\n-- Deteksi: telusuri wait-for graph --\n");
        uint32_t victim = 0;
        if (dl_find_cycle(&victim)) {
            console_set_color(COLOR_ERR);
            kprintf("*** DEADLOCK TERDETEKSI ***\n");
            console_set_color(COLOR_NORMAL);
            kprintf("\n-- Pemulihan: batalkan permintaan mutex milik korban T%u --\n", victim);
            dl_abort(victim);
        } else {
            kprintf("tidak ada deadlock\n");
        }
    }

    for (uint32_t g = 0; !(finished[0] && finished[1]) && g < GUARD; g++) thread_yield();
    for (int i = 0; i < 30; i++) thread_yield();

    kprintf("\n-- Status akhir --\n");
    scheduler_dump();
    console_set_color(COLOR_OK);
    if (fixed) kprintf("\nHASIL: tidak ada deadlock. Circular wait mustahil karena semua thread\n" "mengambil mutex dengan ururtan yang sama (A sebelum B).\n");
    else       kprintf("\nHASIL: deadlock terjadi karena urutan lock berlawanan (A->B vs B->A).\n" "Jalankan 'deadlock' fix untuk melihat solusinya.\n");
    console_set_color(COLOR_NORMAL);
}

void cmd_deadlock(int argc, char **argv) {
    if (argc >= 2 && !strcmp(argv[1], "demo"))     run_demo(false);
    else if (argc >= 2 && !strcmp(argv[1], "fix")) run_demo(true);
    else kprintf("pemakaian: deadlock demo | deadlock fix\n");
}
