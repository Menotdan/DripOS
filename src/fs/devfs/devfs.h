#ifndef DEVFS_H
#define DEVFS_H
#include <stdint.h>
#include "fs/vfs/vfs.h"

void devfs_init();
void register_device(char *name, vfs_ops_t ops);
int devfs_open(char *name, int mode);
int devfs_close(vfs_node_t *node);

#endif