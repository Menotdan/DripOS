#include "devfs.h"
#include "drivers/serial.h"

vfs_node_t *devfs_root;

int devfs_open(char *name, int mode) {
    sprintf("\n[DevFS] Opening %s with mode %d", name, mode);
    return 0;
}

void devfs_init() {
    /* Setup the devfs root */
    devfs_root = vfs_new_node("dev", dummy_ops);
    devfs_root->ops.open = devfs_open;
    vfs_add_child(root_node, devfs_root);
}

void register_device(char *name, vfs_ops_t ops) {
    vfs_node_t *new_device = vfs_new_node(name, ops);
    vfs_add_child(devfs_root, new_device);
}