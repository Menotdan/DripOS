#ifndef FS_FD_H
#define FS_FD_H
#include <stdint.h>
#include "fs/vfs/vfs.h"

int fd_new(vfs_node_t *node);
void fd_remove(int fd);
vfs_node_t *fd_lookup(int fd);

/* FD ops */
int fd_open(char *name, int mode);
int fd_close(int fd);
int fd_read(int fd, void *buf, uint64_t count);
int fd_write(int fd, void *buf, uint64_t count);

#endif