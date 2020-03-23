#include "filesystems.h"
#include "klibc/strhashmap.h"

strhashmap_t *mountpoints;

void register_mountpoint(char *mountpoint, filesystem_gen_node_t node_handler) {
    strhashmap_set_elem(mountpoints, mountpoint, node_handler);
}

uint8_t is_mountpoint(char *path) {
    if (strhashmap_get_elem(mountpoints, path)) {
        return 1;
    }
    return 0;
}

void gen_node_mountpoint(char *mountpoint, char *path) {
    filesystem_gen_node_t node_handler = strhashmap_get_elem(mountpoints, mountpoint);

    if (node_handler) {
        node_handler(path);
    }
}

void init_filesystem_handler() {
    mountpoints = init_strhashmap();
}