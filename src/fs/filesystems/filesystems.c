#include "filesystems.h"
#include "klibc/strhashmap.h"
#include "klibc/stdlib.h"

strhashmap_t *mountpoints;

void register_mountpoint(char *mountpoint, filesystem_gen_node_t node_handler, void *filesystem_descriptor) {
    filesystem_mountpoint_info_t *mountpoint_info = kcalloc(sizeof(filesystem_mountpoint_info_t));
    mountpoint_info->node_handler = node_handler;
    mountpoint_info->filesystem_descriptor = filesystem_descriptor;

    /* Set the mountpoint elem */
    strhashmap_set_elem(mountpoints, mountpoint, mountpoint_info);
}

uint8_t is_mountpoint(char *path) {
    if (strhashmap_get_elem(mountpoints, path)) {
        return 1;
    }
    return 0;
}

void gen_node_mountpoint(char *mountpoint, char *path) {
    filesystem_mountpoint_info_t *node_handler = strhashmap_get_elem(mountpoints, mountpoint);

    if (node_handler) {
        node_handler->node_handler(node_handler->filesystem_descriptor, path);
    }
}

void init_filesystem_handler() {
    mountpoints = init_strhashmap();
}