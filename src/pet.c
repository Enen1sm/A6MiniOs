#include "kernel.h"
#include "scheduler.h"
#include "heap.h"
#include "pet.h"

#define VGA_W        80
#define BAR_ROW      24
#define ZONE_X0      40
#define ZONE_W       (VGA_W - ZONE_X0)
#define SPRITE_W     8
#define MAX_X        (ZONE_W - SPRITE_W)
#define BAR_ATTR     0x1F            

#define HEAP_FULL_PCT   60           
#define SLEEP_TICKS     (15 * TIMER_HZ)
#define FEED_TICKS      (3 * TIMER_HZ)

typedef enum { M_CALM, M_ALERT, M_PANIC, M_FULL, M_SLEEP, M_HAPPY } mood_t;

static const struct {
    const char *face, *suffix, *label;
    uint8_t     attr;
    uint32_t    step;                
} info[] = {
    { "=^.^=", "",    "tenang",                         0x1E, 30 },
    { "=O.O=", " ?",  "waspada (ada thread lain jalan)", 0x1B, 15 },
    { "=@o@=", " !!", "panik (banyak thread aktif)",     0x1C,  5 },
    { "=x.x=", " ~",  "sesak (heap hampir penuh)",       0x1D, 40 },
    { "=-.-=", " zZ", "tidur (lama tidak ada kegiatan)", 0x17, 50 },
    { "=^o^=", " <3", "senang (baru diberi makan)",      0x1A, 20 },
};

static volatile int      pet_alive;
static volatile int      pet_stop;
static volatile uint32_t feed_until;

static mood_t evaluate(uint32_t *heap_pct, int *others) {
    heap_stats_t s;
    heap_get_stats(&s);
    uint32_t pct = s.total ? (s.used * 100u) / s.total : 0;

    int n = (int)scheduler_active_threads() - 2;   
    if (n < 0) n = 0;

    if (heap_pct) *heap_pct = pct;
    if (others)   *others   = n;

    if ((int32_t)(feed_until - timer_ticks()) > 0) return M_HAPPY;
    if (pct >= HEAP_FULL_PCT)                      return M_FULL;
    if (n >= 2)                                    return M_PANIC;
    if (n == 1)                                    return M_ALERT;
    return M_CALM;
}

static volatile uint16_t *const vga = (volatile uint16_t *)0xB8000;

static void zone_clear(void) {
    for (int i = 0; i < ZONE_W; i++)
        vga[BAR_ROW * VGA_W + ZONE_X0 + i] = (uint16_t)(' ' | (BAR_ATTR << 8));
}

static void draw(int x, mood_t m, int frame) {
    char buf[SPRITE_W + 1];
    int n = 0;
    for (const char *p = info[m].face;   *p && n < SPRITE_W; p++) buf[n++] = *p;
    for (const char *p = info[m].suffix; *p && n < SPRITE_W; p++) buf[n++] = *p;
    buf[n] = 0;

    if (frame && m == M_CALM)  buf[2] = '_';                 
    if (frame && m == M_SLEEP) { buf[6] = 'Z'; buf[7] = 'z'; } 

    zone_clear();
    for (int i = 0; i < n; i++)
        vga[BAR_ROW * VGA_W + ZONE_X0 + x + i] =
            (uint16_t)((uint8_t)buf[i] | ((uint16_t)info[m].attr << 8));
}

static void pet_thread(void) {
    int x = 2, dir = 1, frame = 0;
    uint32_t calm_since = timer_ticks();

    while (!pet_stop) {
        uint32_t now = timer_ticks();
        mood_t m = evaluate(0, 0);

        if (m == M_CALM) {
            if ((int32_t)(now - calm_since) >= (int32_t)SLEEP_TICKS) m = M_SLEEP;
        } else {
            calm_since = now;
        }

        draw(x, m, frame);
        frame ^= 1;

        if (m != M_SLEEP) {
            x += dir;
            if (x <= 0)     { x = 0;     dir =  1; }
            if (x >= MAX_X) { x = MAX_X; dir = -1; }
        }

        uint32_t until = now + info[m].step;
        while (!pet_stop && (int32_t)(timer_ticks() - until) < 0)
            thread_yield();          
    }

    zone_clear();
    pet_alive = 0;
}

static void usage(void) {
    kprintf("pemakaian:\n");
    kprintf("   pet          panggil Mochi ke status bar\n");
    kprintf("   pet feed     beri makan (Mochi senang 3 detik)\n");
    kprintf("   pet status   lihat suasana hati Mochi\n");
    kprintf("   pet stop     suruh Mochi pulang\n");
}

void pet_command(int argc, char **argv) {
    const char *sub = (argc >= 2) ? argv[1] : "start";

    if (!strcmp(sub, "start")) {
        if (pet_alive) { kprintf("Mochi sudah ada di status bar. Coba: pet feed\n"); return; }
        pet_stop = 0;
        pet_alive = 1;
        if (thread_create("pet", pet_thread) < 0) {
            pet_alive = 0;
            kprintf("Gagal: tabel thread penuh (maks %d).\n", MAX_THREADS);
            return;
        }
        kprintf("Mochi muncul di pojok kanan status bar. Lihat jendela VGA!\n");
    } else if (!strcmp(sub, "stop")) {
        if (!pet_alive) { kprintf("Mochi tidak sedang ada.\n"); return; }
        pet_stop = 1;
        kprintf("Mochi pamit dulu. Sampai jumpa!\n");
    } else if (!strcmp(sub, "feed")) {
        if (!pet_alive) { kprintf("Panggil Mochi dulu dengan: pet\n"); return; }
        feed_until = timer_ticks() + FEED_TICKS;
        kprintf("Mochi makan dengan lahap. Nyam!\n");
    } else if (!strcmp(sub, "status")) {
        if (!pet_alive) { kprintf("Mochi tidak sedang ada. Panggil dengan: pet\n"); return; }
        uint32_t pct; int others;
        mood_t m = evaluate(&pct, &others);
        kprintf("Mochi: %s\n", info[m].label);
        kprintf("  thread lain aktif : %d\n", others);
        kprintf("  heap terpakai     : %u%%\n", pct);
    } else {
        usage();
    }
}