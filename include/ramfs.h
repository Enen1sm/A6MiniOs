#ifndef RAMFS_H
#define RAMFS_H

#include <stdint.h>

#define RAMFS_MAX       16
#define RAMFS_NAMELEN   20
#define RAMFS_DATALEN   256

enum {
    RAMFS_OK        =  0,
    RAMFS_ENOTFOUND = -1,
    RAMFS_EEXIST    = -2,
    RAMFS_EFULL     = -3,
    RAMFS_ETOOLONG  = -4,
    RAMFS_ENOSPACE  = -5,
};

int         ramfs_create(const char *name);
int         ramfs_write(const char *name, const char *data, uint32_t len);
const char *ramfs_read(const char *name, uint32_t *len);
int         ramfs_delete(const char *name);
int         ramfs_next(int from, const char **name, uint32_t *size);

#endif
