#include "kernel.h"
#include "logo.h"

#define ABOUT_TEXT \
    "Mini OS A6 merupakan proyek tugas mata kuliah Sistem Operasi berbasis bahasa C " \
    "dan Assembly yang dijalankan pada emulator QEMU. Mini OS ini kami buat bertujuan " \
    "untuk memahami cara kerja OS. Proyek ini mengimplementasikan 8 fitur utama arsitektur " \
    "sistem operasi dan tambahan fitur unik. " \

enum { ST_TODO, ST_WORK, ST_DONE };

typedef struct { const char *name; int status; } stage_t;

static const stage_t stages[] = {
    { "Bootstrapping & CLI Kernel",   ST_DONE },
    { "Pengelolaan Proses & Thread",  ST_DONE },
    { "Penjadwalan CPU Preemptive",   ST_DONE },
    { "Sinkronisasi (mutex)",         ST_TODO },
    { "Deadlock",                     ST_TODO },
    { "Alokator Memori Dinamis",      ST_DONE },
    { "Virtual File System",          ST_TODO },
    { "Proteksi & Hak Akses",         ST_TODO },
};
#define NSTAGE ((int)(sizeof(stages) / sizeof(stages[0])))

static void rule(char ch) {
    console_set_color(COLOR_NORMAL);
    for (int i = 0; i < 79; i++) console_putc(ch);
    console_putc('\n');
}

static void tag(int st) {
    console_set_color(COLOR_NORMAL);
    console_putc('[');
    if (st == ST_DONE)      { console_set_color(COLOR_OK);   console_puts(" OK ");   }
    else if (st == ST_WORK) { console_set_color(COLOR_WARN); console_puts("WORK");   }
    else                    { console_set_color(COLOR_NORMAL); console_puts(" -- "); }
    console_set_color(COLOR_NORMAL);
    console_puts("] ");
}

static void print_wrapped(const char *s, int indent, int width) {
    int col = 0;
    for (; col < indent; col++) console_putc(' ');
    while (*s) {
        while (*s == ' ') s++;                       /* lewati spasi */
        int len = 0;
        while (s[len] && s[len] != ' ') len++;       /* panjang kata berikutnya */
        if (len == 0) break;

        int need = len + (col > indent ? 1 : 0);     /* +1 untuk spasi pemisah */
        if (col + need > width) {                    /* tidak muat: baris baru */
            console_putc('\n');
            for (col = 0; col < indent; col++) console_putc(' ');
            need = len;
        }
        if (col > indent) { console_putc(' '); col++; }
        for (int i = 0; i < len; i++) console_putc(s[i]);
        col += len;
        s += len;
    }
    console_putc('\n');
}

static int put_str(char *buf, int n, const char *s) { while (*s) buf[n++] = *s++; return n; }
static int put_num(char *buf, int n, int v)         { buf[n++] = (char)('0' + v); return n; }

static void draw_status_bar(int done) {
    char b[81]; int n = 0;
    n = put_str(b, n, " ");
    n = put_str(b, n, OS_NAME);
    n = put_str(b, n, " OS v");
    n = put_str(b, n, OS_VERSION);
    n = put_str(b, n, " | Modul: ");
    n = put_num(b, n, done);
    n = put_str(b, n, "/");
    n = put_num(b, n, NSTAGE);
    b[n] = 0;
    console_status_bar(b);
}

static void print_stage(int i) {
    tag(stages[i].status);
    kprintf("%d. %-27s", i + 1, stages[i].name);
}

void bootscreen_show(uint32_t magic) {
    int done = 0;
    for (int i = 0; i < NSTAGE; i++) if (stages[i].status == ST_DONE) done++;

    console_clear();

    console_set_color(COLOR_TITLE);
    console_puts(BOOT_LOGO);
    console_set_color(COLOR_OK);
    kprintf("   %s OS v%s - Tugas Besar Sistem Operasi\n", OS_NAME, OS_VERSION);
    console_set_color(COLOR_NORMAL);
    rule('=');

    console_set_color(COLOR_TITLE);
    kprintf("Tentang:\n");
    console_set_color(COLOR_NORMAL);
    print_wrapped(ABOUT_TEXT, 2, 78);

    kprintf("Pemeriksaan boot:\n");
    tag(magic == 0x2BADB002 ? ST_DONE : ST_WORK);
    kprintf("Bootloader Multiboot memuat kernel ke RAM\n");
    tag(ST_DONE); kprintf("Console VGA text 80x25\n");
    tag(ST_DONE); kprintf("Serial COM1 115200 8N1\n");

    console_set_color(COLOR_TITLE);
    kprintf("Progres tugas besar:\n");
    console_set_color(COLOR_NORMAL);
    for (int r = 0; r < NSTAGE / 2; r++) {
        print_stage(r);
        kprintf("  ");
        print_stage(r + NSTAGE / 2);
        kprintf("\n");
    }
    rule('=');

    console_set_color(COLOR_WARN);
    kprintf("Ketik 'help' untuk daftar perintah\n");

    console_set_color(COLOR_OK);
    kprintf("a6> ");
    console_set_color(COLOR_NORMAL);

    draw_status_bar(done);
}