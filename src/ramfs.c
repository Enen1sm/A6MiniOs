#include "kernel.h"
#include "ramfs.h"

struct ramfile {
    bool     used;
    bool     is_protected;
    char     name[RAMFS_NAMELEN];
    uint32_t size;
    char     data[RAMFS_DATALEN];
};

static struct ramfile files[RAMFS_MAX];

static struct ramfile *find(const char *name) {
    for (int i = 0; i < RAMFS_MAX; i++)
        if (files[i].used && strcmp(files[i].name, name) == 0) return &files[i];
    return 0;
}

void ramfs_init(void) {
    static bool inited = false;
    if (inited) return;
    inited = true;
    ramfs_create("system.txt");
    const char *sys_content = "OS_VERSION=A6MiniOS_1.0";
    ramfs_write("system.txt", sys_content, (uint32_t)strlen(sys_content));
    struct ramfile *f = find("system.txt");
    if (f) f->is_protected = true;
}

bool ramfs_is_protected(const char *name) {
    struct ramfile *f = find(name);
    return f ? f->is_protected : false;
}

void ramfs_set_protected(const char *name, bool prot) {
    struct ramfile *f = find(name);
    if (f) f->is_protected = prot;
}

int ramfs_create(const char *name) {
    size_t n = strlen(name);
    if (n == 0 || n >= RAMFS_NAMELEN) return RAMFS_ETOOLONG;
    if (find(name)) return RAMFS_EEXIST;
    for (int i = 0; i < RAMFS_MAX; i++) {
        if (!files[i].used) {
            memset(&files[i], 0, sizeof(files[i]));
            strncpy(files[i].name, name, RAMFS_NAMELEN - 1);
            files[i].used = true;
            return RAMFS_OK;
        }
    }
    return RAMFS_EFULL;
}

int ramfs_write(const char *name, const char *data, uint32_t len) {
    struct ramfile *f = find(name);
    if (!f) return RAMFS_ENOTFOUND;
    if (len >= RAMFS_DATALEN) return RAMFS_ENOSPACE;
    memcpy(f->data, data, len);
    f->data[len] = 0;
    f->size = len;
    return RAMFS_OK;
}

const char *ramfs_read(const char *name, uint32_t *len) {
    struct ramfile *f = find(name);
    if (!f) return 0;
    if (len) *len = f->size;
    return f->data;
}

int ramfs_delete(const char *name) {
    struct ramfile *f = find(name);
    if (!f) return RAMFS_ENOTFOUND;
    f->used = false;
    f->size = 0;
    return RAMFS_OK;
}

int ramfs_next(int from, const char **name, uint32_t *size) {
    for (int i = from; i < RAMFS_MAX; i++) {
        if (files[i].used) {
            if (name) *name = files[i].name;
            if (size) *size = files[i].size;
            return i + 1;
        }
    }
    return 0;
}
