#include "kernel.h"
#include "scheduler.h"
#include "heap.h"
#include "pet.h"

#define COM1     0x3F8
#define LINE_MAX 128
#define MAX_ARGS 8

/* ---- input serial ---- */
static char serial_getc(void) {
    while (!(inb(COM1 + 5) & 0x01)) { }   /* tunggu sampai ada data */
    return (char)inb(COM1);
}

/* baca satu baris sampai Enter */
static void readline(char *buf, int max) {
    int n = 0;
    for (;;) {
        char c = serial_getc();
        if (c == '\r' || c == '\n') {
            console_putc('\n');
            buf[n] = 0;
            return;
        }
        if (c == 0x7F || c == '\b') {      /* backspace */
            if (n > 0) { n--; console_puts("\b \b"); }
            continue;
        }
        if (c >= 32 && c < 127 && n < max - 1) {
            buf[n++] = c;
            console_putc(c);               /* echo */
        }
    }
}

/* pecah "echo halo dunia" jadi argv[] */
static int tokenize(char *line, char **argv, int max) {
    int argc = 0;
    while (*line) {
        while (*line == ' ') *line++ = 0;
        if (!*line) break;
        if (argc < max) argv[argc++] = line;
        while (*line && *line != ' ') line++;
    }
    return argc;
}

/* ---- perintah ---- */
typedef struct {
    const char *name;
    const char *help;
    void (*fn)(int argc, char **argv);
} cmd_t;

static void cmd_help(int argc, char **argv);
static void cmd_clear(int argc, char **argv);
static void cmd_about(int argc, char **argv);
static void cmd_echo(int argc, char **argv);
static void cmd_version(int argc, char **argv);
static void cmd_ps(int argc, char **argv);
static void cmd_quantum(int argc, char **argv);
static void cmd_schedtest(int argc, char **argv);
static void cmd_pet(int argc, char **argv);

static const cmd_t cmds[] = {
    { "help",    "tampilkan daftar perintah", cmd_help    },
    { "clear",   "bersihkan layar",           cmd_clear   },
    { "about",   "tentang A6 OS",             cmd_about   },
    { "echo",    "cetak teks",                cmd_echo    },
    { "version", "versi OS",                  cmd_version },
    { "ps",        "daftar thread dan statusnya", cmd_ps },
    { "quantum",   "lihat/ubah quantum: quantum [tick]", cmd_quantum },
    { "schedtest", "jalankan 2 thread demo Round Robin", cmd_schedtest },
    {"mem",        "memori heap (mem test, mem map)", cmd_mem },
    { "pet",       "hewan peliharaan di status bar (pet [feed|status|stop])", cmd_pet },
};
#define NCMDS (sizeof(cmds) / sizeof(cmds[0]))

static void cmd_help(int argc, char **argv) {
    (void)argc; (void)argv;
    for (unsigned i = 0; i < NCMDS; i++) {
        console_puts("  ");
        console_puts(cmds[i].name);
        console_puts(" - ");
        console_puts(cmds[i].help);
        console_putc('\n');
    }
}
static void cmd_clear(int argc, char **argv) {
    (void)argc; (void)argv;
    console_clear();
}
static void cmd_about(int argc, char **argv) {
    (void)argc; (void)argv;
    console_puts("A6 OS - Tugas Besar Sistem Operasi (C & Assembly, QEMU)\n");
}
static void cmd_echo(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        console_puts(argv[i]);
        if (i < argc - 1) console_putc(' ');
    }
    console_putc('\n');
}
static void cmd_version(int argc, char **argv) {
    (void)argc; (void)argv;
    console_puts("A6 OS v" OS_VERSION "\n");
}

static void cmd_ps(int argc, char **argv) {
    (void)argc; (void)argv;
    scheduler_dump();
}
static void cmd_quantum(int argc, char **argv) {
    if (argc >= 2) {
        uint32_t n = 0;
        for (const char *p = argv[1]; *p; p++) {
            if (*p < '0' || *p > '9') { kprintf("Pakai: quantum <tick>\n"); return; }
            n = n * 10 + (uint32_t)(*p - '0');
            if (n > 1000) { kprintf("Maksimum 1000 tick\n"); return; }
        }
        scheduler_set_quantum(n);
    }
    kprintf("Quantum: %u tick = %u ms\n", scheduler_get_quantum(),
            scheduler_get_quantum() * 1000 / TIMER_HZ);
}
static void cmd_schedtest(int argc, char **argv) {
    (void)argc; (void)argv;
    sched_demo_start();
    kprintf("Thread task_A dan task_B dibuat. Ketik 'ps' untuk melihat status.\n");
}

static void cmd_pet(int argc, char **argv) {
    pet_command(argc, argv);
}

/* cari perintah di tabel lalu jalankan */
static void execute(char *line) {
    char *argv[MAX_ARGS];
    int argc = tokenize(line, argv, MAX_ARGS);
    if (argc == 0) return;
    for (unsigned i = 0; i < NCMDS; i++) {
        if (strcmp(argv[0], cmds[i].name) == 0) {
            cmds[i].fn(argc, argv);
            return;
        }
    }
    console_set_color(COLOR_ERR);
    console_puts("Perintah tidak dikenal: ");
    console_puts(argv[0]);
    console_putc('\n');
    console_set_color(COLOR_NORMAL);
}

void shell_run(void) {
    char line[LINE_MAX];
    for (;;) {
        readline(line, LINE_MAX);
        execute(line);
        console_puts("a6> ");
    }
}
