#ifndef VFS_H
#define VFS_H
#include <stdint.h>
#include "fs/filesystems/filesystems.h"

struct vfs_node;
typedef struct vfs_node vfs_node_t;

/* VFS op types */
typedef int (*vfs_open_t)(char *, int);
typedef int (*vfs_close_t)(int);;
typedef int (*vfs_read_t)(int, void *, uint64_t);
typedef int (*vfs_write_t)(int, void *, uint64_t);
typedef int (*vfs_seek_t)(int, uint64_t, int);

typedef struct {
    vfs_open_t open;
    vfs_close_t close;
    vfs_read_t read;
    vfs_write_t write;
    vfs_seek_t seek;
} vfs_ops_t;

typedef struct vfs_node {
    char *name;
    vfs_ops_t ops;

    void (*node_handle)(struct vfs_node *, struct vfs_node *, char *);

    struct vfs_node *parent; // Parent
    struct vfs_node *fs_root; // Filesystem root
    struct vfs_node **children; // An array of children
    
    uint64_t children_array_size;

    uint64_t unid; // Unique node id
} vfs_node_t;

void vfs_init();
vfs_node_t *vfs_new_node(char *name, vfs_ops_t ops);
void vfs_add_child(vfs_node_t *parent, vfs_node_t *child);
vfs_node_t *create_missing_nodes_from_path(char *path, vfs_ops_t ops);
vfs_node_t *get_node_from_path(char *path);
char *get_full_path(vfs_node_t *node);

void set_child_ops(vfs_node_t *node, vfs_ops_t ops);
void set_child_fs_root(vfs_node_t *node, vfs_node_t *fs_root);

/* VFS ops */
vfs_node_t *vfs_open(char *name, int mode);
int vfs_close(int fd);
int vfs_read(int fd, void *buf, uint64_t count);
int vfs_write(int fd, void *buf, uint64_t count);
int vfs_seek(int fd, uint64_t offset, int whence);

extern vfs_node_t *root_node;
extern vfs_ops_t dummy_ops;

#endif