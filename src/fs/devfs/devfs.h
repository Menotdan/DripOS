#ifndef DEVFS_H
#define DEVFS_H
#include <stdint.h>
#include "fs/vfs/vfs.h"

void devfs_init();
void register_device(char *name, vfs_ops_t ops, void *device_data);
void *get_device_data(vfs_node_t *node);
int devfs_open(char *name, int mode);
int devfs_close(vfs_node_t *node);

void devfs_sprint_devices();

#endif