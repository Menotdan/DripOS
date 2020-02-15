#ifndef VFS_H
#define VFS_H
#include <stdint.h>

typedef struct {
    int (*open)(uint64_t);
} vfs_ops_t;

#endif