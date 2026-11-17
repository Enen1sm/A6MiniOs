#ifndef DEADLOCK_H
#define DEADLOCK_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    const char *name;
    int         rank;
    int         owner;
} dl_mutex_t;

void dl_mutex_init(dl_mutex_t *m, const char *name, int rank);
int dl_lock(dl_mutex_t *m);
void dl_unlock(dl_mutex_t *m);

bool dl_find_cycle(uint32_t *victim);
void dl_abort(uint32_t victim);
int dl_count_waiting(void);

void cmd_deadlock(int argc, char **argv);

#endif