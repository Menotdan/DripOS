#include "filesystems.h"
#include "klibc/strhashmap.h"
#include "klibc/hashmap.h"
#include "klibc/stdlib.h"

hashmap_t *unid_mounts;

void register_unid(uint64_t unid, void *filesystem_dat) {
    hashmap_set_elem(unid_mounts, unid, filesystem_dat);
}

void *get_unid_fs_data(uint64_t unid) {
    return hashmap_get_elem(unid_mounts, unid);
}

void init_filesystem_handler() {
    unid_mounts = init_hashmap();
}