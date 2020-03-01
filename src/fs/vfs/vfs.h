#ifndef VFS_H
#define VFS_H
#include <stdint.h>

struct vfs_node;
typedef struct vfs_node vfs_node_t;

typedef struct {
    int (*open)(char *, int);
    int (*close)(vfs_node_t *);
    int (*read)(vfs_node_t *, void *, uint64_t);
    int (*write)(vfs_node_t *, void *, uint64_t);
} vfs_ops_t;

typedef struct vfs_node {
    char *name;
    vfs_ops_t ops;
    struct vfs_node *parent; // Parent
    struct vfs_node **children; // An array of children
    uint64_t children_array_size;
} vfs_node_t;

void vfs_init();
void vfs_test();
vfs_node_t *get_node_from_path(char *path);

/* VFS ops */
vfs_node_t *vfs_open(char *name, int mode);
void vfs_close(vfs_node_t *node);
void vfs_read(vfs_node_t *node, void *buf, uint64_t count);
void vfs_write(vfs_node_t *node, void *buf, uint64_t count);

#endif