#include "kernel.h"
#include "scheduler.h"
#include "heap.h"
#include "deadlock.h"
#include "pet.h"
#include "keyboard.h"
#include "sync.h"
#include "ramfs.h"

#define COM1     0x3F8
#define LINE_MAX 128
#define MAX_ARGS 8

static int shell_getkey(void) {
    for (;;) {
        if (inb(COM1 + 5) & 0x01) {
            uint8_t c = inb(COM1);
            if (c == 27) {
                int has_next = 0;
                for (volatile int i = 0; i < 20000; i++) {
                    if (inb(COM1 + 5) & 0x01) { has_next = 1; break; }
                }
                if (has_next) {
                    uint8_t c2 = inb(COM1);
                    if (c2 == '[') {
                        for (volatile int j = 0; j < 50000; j++) {
                            if (inb(COM1 + 5) & 0x01) {
                                uint8_t c3 = inb(COM1);
                                if (c3 == 'D') return KEY_LEFT;
                                if (c3 == 'C') return KEY_RIGHT;
                                break;
                            }
                        }
                    }
                }
                return KEY_ESC;
            }
            if (c == '\r') return '\n';
            if (c == 0x7F) return '\b';
            return (int)c;
        }
        int k = keyboard_poll();
        if (k >= 0) {
            if (k == '\r') return '\n';
            if (k == 0x7F) return '\b';
            return k;
        }
        thread_yield();
        __asm__ volatile ("sti; hlt");
    }
}

/* baca satu baris sampai Enter */
static void readline(char *buf, int max) {
    int n = 0;
    for (;;) {
        int k = shell_getkey();
        if (k == '\r' || k == '\n') {
            console_putc('\n');
            buf[n] = 0;
            return;
        }
        if (k == 0x7F || k == '\b') {      /* backspace */
            if (n > 0) { n--; console_puts("\b \b"); }
            continue;
        }
        if (k >= 32 && k < 127 && n < max - 1) {
            buf[n++] = (char)k;
            console_putc((char)k);               /* echo */
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
static void cmd_echo(int argc, char **argv);
static void cmd_version(int argc, char **argv);
static void cmd_role(int argc, char **argv);
static void cmd_ps(int argc, char **argv);
static void cmd_quantum(int argc, char **argv);
static void cmd_schedtest(int argc, char **argv);
static void cmd_pet(int argc, char **argv);
static void cmd_race(int argc, char **argv);
static void cmd_ticktest(int argc, char **argv);
static void cmd_ls(int argc, char **argv);
static void cmd_create(int argc, char **argv);
static void cmd_write(int argc, char **argv);
static void cmd_update(int argc, char **argv);
static void cmd_cat(int argc, char **argv);
static void cmd_rm(int argc, char **argv);

static const cmd_t cmds[] = {
    { "help",    "tampilkan daftar perintah", cmd_help    },
    { "clear",   "bersihkan layar",           cmd_clear   },
    { "echo",    "cetak teks",                cmd_echo    },
    { "version", "versi OS",                  cmd_version },
    { "role",    "ganti/lihat role aktif: role [admin|user]", cmd_role },
    { "ps",        "daftar thread dan statusnya", cmd_ps },
    { "quantum",   "lihat/ubah quantum: quantum [tick]", cmd_quantum },
    { "schedtest", "jalankan 2 thread demo Round Robin", cmd_schedtest },
    {"mem",        "memori heap (mem test, mem map)", cmd_mem },
    { "pet",       "hewan peliharaan di status bar (pet [feed|status|stop])", cmd_pet },
    { "deadlock",  "demo deadlock (deadlock demo | deadlock fix)", cmd_deadlock },
    { "race",      "demo race condition (race nolock|lock|status)", cmd_race },
    { "ticktest",  "uji 1 tick = 10 ms dengan stopwatch: ticktest [detik]", cmd_ticktest },
    { "ls",        "daftar berkas di RAMDisk",                cmd_ls     },
    { "create",    "buat berkas: create <nama>",              cmd_create },
    { "write",     "tulis isi: write <nama> <teks>",          cmd_write  },
    { "update",    "edit isi: update <nama>",                 cmd_update },
    { "cat",       "baca berkas: cat <nama>",                 cmd_cat    },
    { "rm",        "hapus berkas: rm <nama>",                 cmd_rm     },
};
#define NCMDS (sizeof(cmds) / sizeof(cmds[0]))

/* tampilan help*/
typedef struct {
    const char *title;
    const char *names[8];
} help_group_t;

static const help_group_t help_groups[] = {
    { "Sistem",       { "help", "clear", "echo", "version", "role", 0 } },
    { "Proses",       { "ps", "quantum", "schedtest", "ticktest", 0 } },
    { "Sinkronisasi", { "race", 0 } },
    { "Memori",       { "mem", 0 } },
    { "Berkas",       { "ls", "create", "write", "update", "cat", "rm", 0 } },
};
#define NGROUPS (sizeof(help_groups) / sizeof(help_groups[0]))

static const cmd_t *find_cmd(const char *name) {
    for (unsigned i = 0; i < NCMDS; i++)
        if (strcmp(cmds[i].name, name) == 0) return &cmds[i];
    return 0;
}

static int in_any_group(const char *name) {
    for (unsigned g = 0; g < NGROUPS; g++)
        for (int k = 0; help_groups[g].names[k]; k++)
            if (strcmp(help_groups[g].names[k], name) == 0) return 1;
    return 0;
}

/* cetak satu baris: "  nama      - keterangan" dengan kolom rata */
static void print_cmd(const cmd_t *c) {
    console_puts("  ");
    console_puts(c->name);
    for (unsigned i = (unsigned)strlen(c->name); i < 10; i++) console_putc(' ');
    console_puts("- ");
    console_puts(c->help);
    console_putc('\n');
}

static void print_title(const char *t) {
    console_set_color(COLOR_TITLE);
    console_puts(t);
    console_putc('\n');
    console_set_color(COLOR_NORMAL);
}

static void cmd_help(int argc, char **argv) {
    (void)argc; (void)argv;

    for (unsigned g = 0; g < NGROUPS; g++) {
        print_title(help_groups[g].title);
        for (int k = 0; help_groups[g].names[k]; k++) {
            const cmd_t *c = find_cmd(help_groups[g].names[k]);
            if (c) print_cmd(c);
        }
    }

    int header_printed = 0;
    for (unsigned i = 0; i < NCMDS; i++) {
        if (in_any_group(cmds[i].name)) continue;
        if (!header_printed) { print_title("Lainnya"); header_printed = 1; }
        print_cmd(&cmds[i]);
    }
}
static void cmd_clear(int argc, char **argv) {
    (void)argc; (void)argv;
    console_clear();
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

static const char *current_role = "user";

static void cmd_role(int argc, char **argv) {
    if (argc < 2) {
        kprintf("Role aktif: %s\nPakai: role [admin|user]\n", current_role);
        return;
    }
    if (strcmp(argv[1], "admin") == 0) {
        current_role = "admin";
        kprintf("Beralih ke role admin.\n");
    } else if (strcmp(argv[1], "user") == 0) {
        current_role = "user";
        kprintf("Beralih ke role user.\n");
    } else {
        kprintf("Role tidak dikenal. Pilihan: admin, user\n");
    }
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

/* ---- RAMDisk CRUD ---- */
static const char *ramfs_err(int rc) {
    switch (rc) {
    case RAMFS_ENOTFOUND: return "berkas tidak ada";
    case RAMFS_EEXIST:    return "nama sudah dipakai";
    case RAMFS_EFULL:     return "RAMDisk penuh (maks 16 berkas)";
    case RAMFS_ETOOLONG:  return "nama kosong atau lebih dari 19 karakter";
    case RAMFS_ENOSPACE:  return "isi melebihi 255 byte";
    default:              return "gagal";
    }
}

static void cmd_ls(int argc, char **argv) {
    (void)argc; (void)argv;
    const char *name; uint32_t size;
    int i = 0, n = 0;
    while ((i = ramfs_next(i, &name, &size)) != 0) {
        kprintf("  %-20s %u byte\n", name, size);
        n++;
    }
    if (!n) kprintf("  (belum ada berkas)\n");
}

static void make_proc_name(char *dst, size_t max, const char *op, const char *arg) {
    size_t i = 0;
    while (*op && i + 1 < max) dst[i++] = *op++;
    if (arg && *arg && i + 1 < max) dst[i++] = ' ';
    if (arg) {
        while (*arg && i + 1 < max) dst[i++] = *arg++;
    }
    dst[i] = 0;
}

static void editor_render(const char *prefix, const char *buf, int len, int cursor_pos, int *prev_len) {
    console_putc('\r');
    int count = 0;
    for (const char *p = prefix; *p; p++) { console_putc(*p); count++; }
    for (int i = 0; i <= len; i++) {
        if (i == cursor_pos) { console_putc('|'); count++; }
        if (i < len) { console_putc(buf[i]); count++; }
    }
    if (count < *prev_len) {
        int diff = *prev_len - count;
        for (int i = 0; i < diff; i++) console_putc(' ');
        console_putc('\r');
        for (const char *p = prefix; *p; p++) console_putc(*p);
        for (int i = 0; i <= len; i++) {
            if (i == cursor_pos) console_putc('|');
            if (i < len) console_putc(buf[i]);
        }
    }
    *prev_len = count;
}

static void deny_msg(const char *name, const char *action) {
    console_set_color(COLOR_ERR);
    if (strcmp(name, "system.txt") == 0) {
        kprintf("[ACCESS DENIED] File sistem hanya bisa diubah atau dihapus oleh admin.\n");
    } else {
        kprintf("[ACCESS DENIED] Berkas terproteksi hanya bisa %s oleh admin.\n", action);
    }
    console_set_color(COLOR_NORMAL);
}

static void cmd_create(int argc, char **argv) {
    if (argc < 2) { kprintf("pakai: create <nama>\n"); return; }
    char pname[THREAD_NAME_LEN];
    make_proc_name(pname, sizeof(pname), "create", argv[1]);
    int pid = process_register(pname);

    int rc = ramfs_create(argv[1]);
    if (rc == RAMFS_OK) {
        if (strcmp(current_role, "admin") == 0) {
            ramfs_set_protected(argv[1], true);
            kprintf("dibuat (terproteksi admin): %s\n", argv[1]);
        } else {
            kprintf("dibuat: %s\n", argv[1]);
        }
    } else {
        console_set_color(COLOR_ERR);
        kprintf("gagal: %s\n", ramfs_err(rc));
        console_set_color(COLOR_NORMAL);
    }

    process_terminate(pid);
}

static void cmd_write(int argc, char **argv) {
    if (argc < 3) { kprintf("pakai: write <nama> <teks>\n"); return; }
    char pname[THREAD_NAME_LEN];
    make_proc_name(pname, sizeof(pname), "write", argv[1]);
    int pid = process_register(pname);

    if (ramfs_is_protected(argv[1]) && strcmp(current_role, "admin") != 0) {
        deny_msg(argv[1], "diubah atau dihapus");
        process_terminate(pid);
        return;
    }

    char buf[RAMFS_DATALEN]; int n = 0;
    for (int i = 2; i < argc; i++) {
        if (i > 2) {
            if (n >= RAMFS_DATALEN - 1) {
                console_set_color(COLOR_ERR);
                kprintf("gagal: isi melebihi 255 byte\n");
                console_set_color(COLOR_NORMAL);
                process_terminate(pid);
                return;
            }
            buf[n++] = ' ';
        }
        for (const char *p = argv[i]; *p; p++) {
            if (n >= RAMFS_DATALEN - 1) {
                console_set_color(COLOR_ERR);
                kprintf("gagal: isi melebihi 255 byte\n");
                console_set_color(COLOR_NORMAL);
                process_terminate(pid);
                return;
            }
            buf[n++] = *p;
        }
    }
    int rc = ramfs_write(argv[1], buf, (uint32_t)n);
    if (rc == RAMFS_OK) kprintf("%s: %d byte ditulis\n", argv[1], n);
    else { console_set_color(COLOR_ERR); kprintf("gagal: %s\n", ramfs_err(rc)); console_set_color(COLOR_NORMAL); }

    process_terminate(pid);
}

static void cmd_update(int argc, char **argv) {
    if (argc < 2) { kprintf("pakai: update <nama>\n"); return; }
    char pname[THREAD_NAME_LEN];
    make_proc_name(pname, sizeof(pname), "update", argv[1]);
    int pid = process_register(pname);

    if (ramfs_is_protected(argv[1]) && strcmp(current_role, "admin") != 0) {
        deny_msg(argv[1], "diubah atau dihapus");
        process_terminate(pid);
        return;
    }

    uint32_t orig_len = 0;
    const char *orig_data = ramfs_read(argv[1], &orig_len);
    if (!orig_data) {
        console_set_color(COLOR_ERR);
        kprintf("gagal: %s\n", ramfs_err(RAMFS_ENOTFOUND));
        console_set_color(COLOR_NORMAL);
        process_terminate(pid);
        return;
    }

    char buf[RAMFS_DATALEN];
    int len = (int)orig_len;
    if (len >= RAMFS_DATALEN) len = RAMFS_DATALEN - 1;
    memcpy(buf, orig_data, (size_t)len);
    buf[len] = 0;
    int cursor_pos = len;
    int prev_len = 0;

    editor_render("edit: ", buf, len, cursor_pos, &prev_len);

    for (;;) {
        int k = shell_getkey();
        if (k == KEY_ESC) {
            console_putc('\n');
            kprintf("edit dibatalkan\n");
            process_terminate(pid);
            return;
        }
        if (k == '\n') {
            console_putc('\n');
            int rc = ramfs_write(argv[1], buf, (uint32_t)len);
            if (rc == RAMFS_OK) {
                kprintf("%s: %d byte disimpan\n", argv[1], len);
            } else {
                console_set_color(COLOR_ERR);
                kprintf("gagal: %s\n", ramfs_err(rc));
                console_set_color(COLOR_NORMAL);
            }
            process_terminate(pid);
            return;
        }
        if (k == KEY_LEFT) {
            if (cursor_pos > 0) cursor_pos--;
            editor_render("edit: ", buf, len, cursor_pos, &prev_len);
        } else if (k == KEY_RIGHT) {
            if (cursor_pos < len) cursor_pos++;
            editor_render("edit: ", buf, len, cursor_pos, &prev_len);
        } else if (k == '\b') {
            if (cursor_pos > 0) {
                for (int i = cursor_pos - 1; i < len - 1; i++) {
                    buf[i] = buf[i + 1];
                }
                len--;
                cursor_pos--;
                buf[len] = 0;
                editor_render("edit: ", buf, len, cursor_pos, &prev_len);
            }
        } else if (k >= 32 && k <= 126) {
            if (len < RAMFS_DATALEN - 1) {
                for (int i = len; i > cursor_pos; i--) {
                    buf[i] = buf[i - 1];
                }
                buf[cursor_pos] = (char)k;
                len++;
                cursor_pos++;
                buf[len] = 0;
                editor_render("edit: ", buf, len, cursor_pos, &prev_len);
            }
        }
    }
}

static void cmd_cat(int argc, char **argv) {
    if (argc < 2) { kprintf("pakai: %s <nama>\n", argv[0]); return; }
    char pname[THREAD_NAME_LEN];
    make_proc_name(pname, sizeof(pname), argv[0], argv[1]);
    int pid = process_register(pname);

    if (ramfs_is_protected(argv[1]) && strcmp(argv[1], "system.txt") != 0 && strcmp(current_role, "admin") != 0) {
        deny_msg(argv[1], "dibuka");
        process_terminate(pid);
        return;
    }

    uint32_t n = 0;
    const char *data = ramfs_read(argv[1], &n);
    if (!data) {
        console_set_color(COLOR_ERR);
        kprintf("gagal: %s\n", ramfs_err(RAMFS_ENOTFOUND));
        console_set_color(COLOR_NORMAL);
        process_terminate(pid);
        return;
    }
    kprintf("%s (%u byte):\n", argv[1], n);
    console_puts(data);
    console_putc('\n');

    process_terminate(pid);
}

static void cmd_rm(int argc, char **argv) {
    if (argc < 2) { kprintf("pakai: %s <nama>\n", argv[0]); return; }
    char pname[THREAD_NAME_LEN];
    make_proc_name(pname, sizeof(pname), argv[0], argv[1]);
    int pid = process_register(pname);

    if (ramfs_is_protected(argv[1]) && strcmp(current_role, "admin") != 0) {
        deny_msg(argv[1], "diubah atau dihapus");
        process_terminate(pid);
        return;
    }

    int rc = ramfs_delete(argv[1]);
    if (rc == RAMFS_OK) kprintf("dihapus: %s\n", argv[1]);
    else { console_set_color(COLOR_ERR); kprintf("gagal: %s\n", ramfs_err(rc)); console_set_color(COLOR_NORMAL); }

    process_terminate(pid);
}

static void cmd_race(int argc, char **argv) {
    race_command(argc, argv);
}

/* Menunggu N detik berdasarkan hitungan tick. Cocokkan dengan stopwatch. */
static void cmd_ticktest(int argc, char **argv) {
    uint32_t detik = 10;
    if (argc >= 2) {
        detik = 0;
        for (const char *p = argv[1]; *p; p++) {
            if (*p < '0' || *p > '9') { detik = 0; break; }
            detik = detik * 10 + (uint32_t)(*p - '0');
        }
        if (detik < 1 || detik > 60) { kprintf("Pakai: ticktest [1..60 detik]\n"); return; }
    }
    kprintf("%u detik = %u tick (1 tick = %u ms)\n", detik, detik * TIMER_HZ, 1000 / TIMER_HZ);
    kprintf(">>> MULAI - nyalakan stopwatch sekarang\n");
    uint32_t awal = timer_ticks();
    while (timer_ticks() - awal < detik * TIMER_HZ) { }
    kprintf(">>> SELESAI - hentikan stopwatch. Harusnya %u detik.\n", detik);
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

static void print_prompt(void) {
    console_set_color(COLOR_OK);
    kprintf("MiniOS [%s]> ", current_role);
    console_set_color(COLOR_NORMAL);
}

void shell_run(void) {
    ramfs_init();
    char line[LINE_MAX];
    for (;;) {
        readline(line, LINE_MAX);
        execute(line);
        print_prompt();
    }
}
