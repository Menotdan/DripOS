#include "devfs.h"
#include "drivers/serial.h"
#include "klibc/hashmap.h"

vfs_node_t *devfs_root;
hashmap_t devfs_hashmap;

int devfs_open(char *name, int mode) {
    sprintf("\n[DevFS] Opening %s with mode %d", name, mode);
    return 0;
}

int devfs_close(fd_entry_t *fd_data) {
    vfs_node_t *node = fd_data->node;

    sprintf("\n[DevFS] Closing %s", node->name);
    return 0;
}

void devfs_init() {
    /* Setup the devfs root */
    devfs_root = vfs_new_node("dev", dummy_ops);
    devfs_root->ops.open = devfs_open;
    vfs_add_child(root_node, devfs_root);

    /* Setup the devfs hashmap */
    devfs_hashmap.bucket_highest = 0;
    devfs_hashmap.buckets = (void *) 0;
    devfs_hashmap.hashmap_lock = 0;
}

void devfs_sprint_devices() {
    sprintf("\n[DevFS] Device List:");
    for (uint64_t i = 0; i < devfs_root->children_array_size; i++) {
        vfs_node_t *child = devfs_root->children[i];
        if (child) {
            sprintf("\n  Device %s", child->name);
        }
    }
}

void register_device(char *name, vfs_ops_t ops, void *device_data) {
    vfs_node_t *new_device = vfs_new_node(name, ops);
    vfs_add_child(devfs_root, new_device);
    
    hashmap_set_elem(&devfs_hashmap, new_device->unid, device_data);
}

void *get_device_data(vfs_node_t *node) {
    return hashmap_get_elem(&devfs_hashmap, node->unid);
}

