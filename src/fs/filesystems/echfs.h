#ifndef ECHFS_FILESYSTEM_H
#define ECHFS_FILESYSTEM_H
#include <stdint.h>
#include "klibc/lock.h"
#include "klibc/hashmap.h"
#include "fs/vfs/vfs.h"

#define BYTES_TO_BLOCKS(bytes, block_size) ((bytes + block_size - 1) / block_size)
#define BLOCKS_TO_BYTES(blocks, block_size) (blocks * block_size)

#define ECHFS_END_OF_CHAIN 0xFFFFFFFFFFFFFFFF
#define ECHFS_DELETED_ENTRY 0xFFFFFFFFFFFFFFFE

#define ECHFS_ROOT_DIR_ID 0xFFFFFFFFFFFFFFFF
#define ECHFS_SEARCH_FAIL 0xFFFFFFFFFFFFFFFF

#define ECHFS_TYPE_DIR 1
#define ECHFS_TYPE_FILE 0

typedef struct {
    uint8_t jmp[4]; // Jump instruction
    char sig[8]; // Echfs signature
    uint64_t block_count;
    uint64_t main_dir_blocks;
    uint64_t block_size;
} __attribute__((packed)) echfs_block0_t;

typedef struct {
    uint64_t parent_id;

    uint8_t entry_type;
    char name[201];

    uint64_t unix_access_time;
    uint64_t unix_modify_time;

    uint16_t permissions;
    uint16_t owner_id;
    uint16_t group_id;

    uint64_t unix_create_time;

    uint64_t starting_block; // Or directory id if it's a directory
    uint64_t file_size_bytes;

    // added for usefulness, not actually in the entry
    uint64_t entry_number;
} __attribute__((packed)) echfs_dir_entry_t;

typedef struct {
    uint64_t alloc_table_addr;
    uint64_t alloc_table_size;
    uint64_t alloc_table_block;
    uint64_t alloc_table_blocks;
    uint64_t main_dir_block;
    uint64_t main_dir_blocks;
    uint64_t blocks;
    uint64_t block_size;
    char *device_name;
    char *mountpoint_path;
    vfs_node_t *mountpoint;

    hashmap_t *cached_blocks;
    hashmap_t *cached_dir_entries;
    lock_t cache_block_lock;
    lock_t cache_dir_lock;
} echfs_filesystem_t;

int echfs_check_and_init(char *device, echfs_filesystem_t *output);
void echfs_test(char *device);

#endif