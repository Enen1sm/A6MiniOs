#ifndef HEAP_H
#define HEAP_H

#include <stdint.h>
#include <stddef.h>

#define HEAP_SIZE (1024 * 1024)

typedef struct {
    uint32_t total;
    uint32_t used;
    uint32_t free;
    uint32_t overhead;
    uint32_t largest_free;
    uint32_t used_blocks;
    uint32_t free_blocks;
    uint32_t alloc_ok;
    uint32_t alloc_fail;
    uint32_t free_ok;
    uint32_t free_bad;
    int corrupted;
} heap_stats_t;

void heap_init(void);

void *kmalloc(size_t n);

void *kcalloc(size_t count, size_t size);

void kfree(void *p);

void heap_get_stats(heap_stats_t *s);

void cmd_mem(int argc, char **argv);

#endif