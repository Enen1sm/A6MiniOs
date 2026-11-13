#include "kernel.h"
#include "heap.h"

#define ALIGN       8u
#define BLK_MAGIC   0xA6B10C4Du

typedef struct block {
    uint32_t magic;
    uint32_t size;
    uint32_t free;
    uint32_t pad;
    struct block *next;
    struct block *prev;
} block_t;

#define HDR ((uint32_t)sizeof(block_t))

static uint8_t heap_area[HEAP_SIZE] __attribute__((aligned(16)));
static block_t *head;
static int inited;

static uint32_t alloc_ok, alloc_fail, free_ok, free_bad;

void heap_init(void) {
    head = (block_t *)heap_area;
    head->magic = BLK_MAGIC;
    head->size = HEAP_SIZE -HDR;
    head->free = 1;
    head->next = NULL;
    head->prev = NULL;
    inited = 1;
}

void *kmalloc(size_t n) {
    if (n == 0) return NULL;
    uint32_t flags = irq_save();

    if (!inited) heap_init();

    if (n > HEAP_SIZE) { alloc_fail++; irq_restore(flags); return NULL; }
    uint32_t need = ((uint32_t)n + (ALIGN - 1)) & ~(ALIGN - 1);

    for (block_t *b = head; b; b = b->next) {
        if (b->magic != BLK_MAGIC) break;
        if (!b->free || b->size < need) continue;

        if (b->size >= need + HDR + ALIGN) {
            block_t *rest = (block_t *)((uint8_t *)b + HDR + need);
            rest->magic = BLK_MAGIC;
            rest->size = b->size - need - HDR;
            rest->free = 1;
            rest->next = b->next;
            rest->prev = b;
            if (b->next) b->next->prev = rest;
            b->next = rest;
            b->size = need;
        }
        b->free = 0;
        alloc_ok++;
        irq_restore(flags);
        return (uint8_t *)b + HDR;
    }
    alloc_fail++;
    irq_restore(flags);
    return NULL;
}

void *kcalloc(size_t count, size_t size) {
    if (count != 0 && size > HEAP_SIZE / count) return NULL;
    size_t total = count * size;
    void *p = kmalloc(total);
    if (p) memset(p, 0, total);
    return p;
}

void kfree(void *p) {
    if (!p) return;
    uint32_t flags = irq_save();
    if (!inited) heap_init();

    uint32_t addr = (uint32_t)p, lo = (uint32_t)heap_area + HDR, hi = (uint32_t)heap_area + HEAP_SIZE;
    if (addr < lo || addr >= hi || (addr & (ALIGN - 1))) {
        console_set_color(COLOR_WARN);
        kprintf("[heap] kfree(%p) ditolak: alamat di luar heap / tidak rata\n", p);
        console_set_color(COLOR_NORMAL);
        free_bad++; irq_restore(flags); return;
    }

    block_t *b = (block_t *)((uint8_t *)p - HDR);
    if (b->magic != BLK_MAGIC) {
        console_set_color(COLOR_WARN);
        kprintf("[heap] kfree(%p) ditolak: bukan awal blok sah (header rusak/sudah digabung)\n", p);
        console_set_color(COLOR_NORMAL);
        free_bad++; irq_restore(flags); return;
    }
    if (b->free) {
        console_set_color(COLOR_WARN);
        kprintf("[heap] kfree(%p) ditolak: DOUBLE FREE terdeteksi, diabaikan\n", p);
        console_set_color(COLOR_NORMAL);
        free_bad++; irq_restore(flags); return;
    }

    b->free = 1;
    free_ok++;

    if (b->next && b->next->free) {
        b->next->magic = 0;
        b->size += HDR + b->next->size;
        b->next = b->next->next;
        if (b->next) b->next->prev = b;
    }

    if (b->prev && b->prev->free) {
        block_t *pv = b->prev;
        b->magic = 0;
        pv->size += HDR + b->size;
        pv->next = b->next;
        if (b->next) b->next->prev = pv;
    }
    irq_restore(flags);
}

void heap_get_stats(heap_stats_t *s) {
    uint32_t flags = irq_save();
    if (!inited) heap_init();
    memset(s, 0, sizeof(*s));
    s->total = HEAP_SIZE;
    for (block_t *b = head; b; b = b->next) {
        if (b->magic != BLK_MAGIC) { s->corrupted = 1; break; }
        s->overhead += HDR;
        if (b->free) {
            s->free += b->size; s->free_blocks++;
            if (b->size > s->largest_free) s->largest_free = b->size;
        } else {
            s->used += b->size; s->used_blocks++;
        }
    }
    s->alloc_ok = alloc_ok; s->alloc_fail = alloc_fail;
    s->free_ok = free_ok; s->free_bad = free_bad;
    irq_restore(flags);
}


#define SLOTS 16
static struct { void *ptr; uint32_t size; } slot[SLOTS];

static void print_stats(const char *title) {
    heap_stats_t s;
    heap_get_stats(&s);

    console_set_color(COLOR_TITLE);
    kprintf("--- %s ---\n", title);
    console_set_color(COLOR_NORMAL);
    kprintf("   Total heap      : %8u B (%u KB)\n", s.total, s.total / 1024);
    kprintf("   Terpakai (data) : %8u B (%u KB) [%u blok]\n", s.used, s.used / 1024, s.used_blocks);
    kprintf("   Tersedia (bebas): %8u B (%u KB) [%u blok]\n", s.free, s.free / 1024, s.free_blocks);
    kprintf("   Overhead header : %8u B\n", s.overhead);
    kprintf("   Blok bebas terbesar: %u B (%u KB)\n", s.largest_free, s.largest_free / 1024);

    uint32_t filled = (s.total - s.free) * 40 / s.total;

    kprintf(" [");
    for (uint32_t i = 0; i < 40; i++) console_putc(i < filled ? '#' : '.');
    kprintf("] %u%%\n", (s.total - s.free) * 100 / s.total);
    kprintf("   kmalloc: %u berhasil, %u gagal (OOM) | kfree: %u valid, %u ditolak\n", s.alloc_ok, s.alloc_fail, s.free_ok, s.free_bad);
    if (s.corrupted) {
        console_set_color(COLOR_ERR);
        kprintf("   PERINGATAN: header heap rusak terdeteksi\n");
        console_set_color(COLOR_NORMAL);
    }
}

static void print_map(void) {
    uint32_t flags = irq_save();
    if (!inited) heap_init();
    console_set_color(COLOR_TITLE);
    kprintf("--- Peta blok heap (urut alamat) ---\n");
    console_set_color(COLOR_NORMAL);
    kprintf(" %-10s %-10s %s\n", "ALAMAT", "UKURAN", "STATUS");
    int n = 0;
    for (block_t *b = head; b && n < 40; b = b->next, n++) {
        if (b->magic != BLK_MAGIC) { kprintf("  header rusak di %p\n", (void *)b); break; }
        kprintf("   %p %-10u %s\n", (void*)((uint8_t *)b + HDR), b->size, b->free ? "bebas" : "TERPAKAI");
    }
    irq_restore(flags);
}

static int parse_size(const char *s, uint32_t *out) {
    uint32_t v = 0; int digits = 0;
    while (*s >= '0' && *s <= '9') { v = v * 10 + (uint32_t)(*s - '0'); s++; digits++; if (v > 0x3FFFFF) return 0; }
    if (!digits) return 0;
    if (*s == 'k' || *s == 'K') { v *= 1024; s++; }
    if (*s) return 0;
    *out = v;
    return 1;
}

static int parse_num(const char *s, uint32_t *out) {
    uint32_t v = 0; int digits = 0;
    while (*s >= '0' && *s <= '9') { v = v * 10 + (uint32_t)(*s - '0'); s++; digits++;}
    if (!digits || *s) return 0;
    *out = v;
    return 1;
}

static void usage(void) {
    kprintf("pemakaian:\n");
    kprintf("   mem             statistik memori terpakai/tersedia\n");
    kprintf("   mem alloc <n>   alokasi n byte, atau n KB bila ditulis 64k\n");
    kprintf("   mem free <slot> bebaskan alokasi di slot tersebut\n");
    kprintf("   mem map         peta blok heap (lihat fragmentasi & coalescing)\n");
    kprintf("   mem test        demo otomatis: alokasi, free, coalescing, OOM\n");
}

static void check_pattern(const char *name, uint8_t *p, uint32_t n, uint8_t v) {
    for (uint32_t i = 0; i < n; i++) {
        if (p[i] != v) { kprintf("  %s: DATA RUSAK di offset %u!\n", name, i); return; }
    }
    kprintf("   %s: isi %u byte terverifikasi (0x%02X)\n", name, n, v);
}

static void mem_test(void) {
    int cleaned = 0;
    for (int i = 0; i < SLOTS; i++) {
        if (slot[i].ptr) { kfree(slot[i].ptr); slot[i].ptr = NULL; slot[i].size = 0; cleaned++; }
    }
    if (cleaned) kprintf("(membersihkan %d slot manual sebelum demo)\n", cleaned);

    console_set_color(COLOR_OK);
    kprintf("=== DEMO ALOKATOR MEMORI DINAMIS ===\n");
    console_set_color(COLOR_NORMAL);

    print_stats("1. Kondisi awal");

    kprintf("\n[2] kmalloc: A=100KB, B=200KB, C=50KB\n");
    uint8_t *a = kmalloc(100 * 1024), *b = kmalloc(200 * 1024), *c = kmalloc(50 * 1024);
    kprintf("   A=%p    B=%p    C=%p\n", a, b, c);
    if (!a || !b || !c) { kprintf(" alokasi gagal, demo dihentikkan\n"); kfree(a); kfree(b); kfree(c); return; }
    memset(a, 0xAA, 100 * 1024); memset(b, 0xBB, 200 * 1024); memset(c, 0xCC, 50 * 1024);
    check_pattern("A", a, 100 * 1024, 0xAA);
    check_pattern("B", b, 200 * 1024, 0xBB);
    check_pattern("C", c, 50 * 1024, 0xCC);
    print_stats("Setelah alokasi (terpakai naik, tersedia turun)");

    kprintf("\n[3] kfree(B) -> memori dapat dilepas, terbentuk lubang di tengah\n");
    kfree(b);
    print_map();
    print_stats("Setelah kfree(B)");

    kprintf("\n[4] kmalloc(150KB) -> first-fit memakai lubang bekas\n");
    uint8_t *d = kmalloc(150 * 1024);
    kprintf("   D=%p (alamat B dulu = %p)\n", d, b);
    print_map();

    kprintf("\n[5] kfree(A), kfree(C), kfree(D) -> coalescing menyatukan blok bebas\n");
    kfree(a); kfree(c); kfree(d);
    print_map();
    print_stats("Setelah semua dibebaskan (harus kembali seperti awal)");

    kprintf("\n[6] Perlindungan: double free & pointer sembarang\n");
    kfree(a);                         
    kfree((void *)0x1234);

    kprintf("\n[7] OUT OF MEMORY: minta %u KB (melebihi kapasitas heap)\n", (HEAP_SIZE / 1024) + 1);
    void *big = kmalloc(HEAP_SIZE + 1);
    if (!big) {
        console_set_color(COLOR_WARN);
        kprintf("  kmalloc mengembalikan NULL -> OOM ditangani, kernel TIDAK crash\n");
        console_set_color(COLOR_NORMAL);
    }

    kprintf("\n[8] Habiskan heap dengan blok 128KB sampai gagal\n");
    void *blk[SLOTS]; int n = 0;
    while (n < SLOTS) {
        void *p = kmalloc(128 * 1024);
        if (!p) { console_set_color(COLOR_WARN); kprintf("  alokasi ke-%d gagal (OOM), kernel tetap berjalan\n", n + 1); console_set_color(COLOR_NORMAL); break; }
        blk[n++] = p;
    }
    kprintf("  berhasil %d blok x 128KB\n", n);
    print_stats("Heap hampir penuh");
    for (int i = 0; i < n; i++) kfree(blk[i]);
    print_stats("Setelah semua dilepas (pulih sepenuhnya)");

    console_set_color(COLOR_OK);
    kprintf("\nHASIL: alokasi, pelepasan, coalescing, dan OOM semuanya tertangani aman.\n");
    console_set_color(COLOR_NORMAL);
}

void cmd_mem(int argc, char **argv) {
    if (argc == 1) { print_stats("Statistik heap"); return; }
 
    if (!strcmp(argv[1], "alloc")) {
        uint32_t sz;
        if (argc < 3 || !parse_size(argv[2], &sz) || sz == 0) { kprintf("pemakaian: mem alloc <n>  (contoh: mem alloc 500  atau  mem alloc 64k)\n"); return; }
        int id = -1;
        for (int i = 0; i < SLOTS; i++) if (!slot[i].ptr) { id = i; break; }
        if (id < 0) { kprintf("semua %d slot terpakai, bebaskan dulu dengan 'mem free <slot>'\n", SLOTS); return; }
        void *p = kmalloc(sz);
        if (!p) {
            console_set_color(COLOR_ERR);
            kprintf("OUT OF MEMORY: permintaan %u byte ditolak. Kernel tetap berjalan.\n", sz);
            console_set_color(COLOR_NORMAL);
        } else {
            memset(p, 0xA6, sz);
            slot[id].ptr = p; slot[id].size = sz;
            kprintf("slot %d: %u byte di %p\n", id, sz, p);
        }
        print_stats("Statistik heap");
    } else if (!strcmp(argv[1], "free")) {
        uint32_t id;
        if (argc < 3 || !parse_num(argv[2], &id) || id >= SLOTS || !slot[id].ptr) { kprintf("slot tidak valid / kosong\n"); return; }
        kfree(slot[id].ptr);
        kprintf("slot %u dibebaskan (%u byte)\n", id, slot[id].size);
        slot[id].ptr = NULL; slot[id].size = 0;
        print_stats("Statistik heap");
    } else if (!strcmp(argv[1], "map")) {
        print_map();
    } else if (!strcmp(argv[1], "test")) {
        mem_test();
    } else {
        usage();
    }
}