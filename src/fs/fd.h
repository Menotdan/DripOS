#ifndef FS_FD_H
#define FS_FD_H
#include <stdint.h>
#include "vfs/vfs.h"

struct vfs_node;
typedef struct vfs_node vfs_node_t;

int fd_new(vfs_node_t *node, int mode);
void fd_remove(int fd);
fd_entry_t *fd_lookup(int fd);

/* FD ops */
int fd_open(char *name, int mode);
int fd_close(int fd);
int fd_read(int fd, void *buf, uint64_t count);
int fd_write(int fd, void *buf, uint64_t count);

#endif