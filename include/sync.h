#ifndef SYNC_H
#define SYNC_H

#include <stdint.h>
#include "scheduler.h"

/* Mutex = "kunci". Hanya satu thread yang boleh memegangnya.
 * Thread lain yang mau mengunci harus menunggu (status WAITING). */
typedef struct {
    int locked;                 /* 0 = bebas, 1 = sedang dikunci */
    int owner;                  /* id thread yang memegang kunci */
    int waiters[MAX_THREADS];   /* daftar thread yang sedang menunggu */
    int n_waiters;
} mutex_t;

void mutex_init(mutex_t *m);
void mutex_lock(mutex_t *m);
int  mutex_unlock(mutex_t *m);  /* 0 = berhasil, -1 = bukan pemilik kunci */

/* Demo race condition (perintah shell "race") */
void race_command(int argc, char **argv);

#endif
