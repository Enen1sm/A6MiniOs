#include "kernel.h"
#include "sync.h"

void mutex_init(mutex_t *m) {
    m->locked = 0;
    m->owner = -1;
    m->n_waiters = 0;
}

/* Interrupt dimatikan sebentar supaya saat memeriksa kunci,
 * thread tidak diganti oleh timer. */
void mutex_lock(mutex_t *m) {
    uint32_t flags = irq_save();

    while (m->locked) {                                  /* kunci dipakai thread lain */
        m->waiters[m->n_waiters++] = (int)thread_current_id();   /* masuk daftar tunggu */
        thread_block();                                  /* status jadi WAITING */
    }
    m->locked = 1;                                       /* kunci sekarang milik kita */
    m->owner = (int)thread_current_id();

    irq_restore(flags);
}

int mutex_unlock(mutex_t *m) {
    uint32_t flags = irq_save();

    if (!m->locked || m->owner != (int)thread_current_id()) {
        irq_restore(flags);
        return -1;                                       /* hanya pemilik yang boleh membuka */
    }
    m->locked = 0;
    m->owner = -1;

    if (m->n_waiters > 0) {                              /* bangunkan penunggu pertama */
        int id = m->waiters[0];
        for (int i = 1; i < m->n_waiters; i++)
            m->waiters[i - 1] = m->waiters[i];
        m->n_waiters--;
        thread_unblock((uint32_t)id);                    /* WAITING -> READY */
    }

    irq_restore(flags);
    return 0;
}
