#include "echfs.h"
#include "fs/fd.h"
#include "klibc/stdlib.h"
#include "klibc/string.h"
#include "klibc/math.h"
#include "klibc/hashmap.h"

#include "mm/pmm.h"

#include "drivers/pit.h"

#include "drivers/serial.h"
#include "proc/scheduler.h"

hashmap_t echfs_mountpoints_map = {0, 0, 0};

/* Parse the first block of information */
int echfs_read_block0(char *device, echfs_filesystem_t *output) {
    echfs_block0_t *block0 = kcalloc(sizeof(echfs_block0_t));
    int device_fd = fd_open(device, 0);

    fd_read(device_fd, block0, sizeof(echfs_block0_t));
    fd_seek(device_fd, 0, 0);

    if (block0->sig[0] == '_' && block0->sig[1] == 'E' && block0->sig[2] == 'C' && block0->sig[3] == 'H' &&
        block0->sig[4] == '_' && block0->sig[5] == 'F' && block0->sig[6] == 'S' && block0->sig[7] == '_') {

        sprintf("\nFound echFS drive.\nBlock count: %lu, Block size: %lu\nMain dir blocks: %lu", block0->block_count, block0->block_size, block0->main_dir_blocks);

        /* Set data for the filesystem structure */

        /* Name and block count/size */

        output->device_name = kcalloc(strlen(device) + 1);
        strcpy(device, output->device_name); // Name
        output->blocks = block0->block_count; // Block count
        output->block_size = block0->block_size; // Block size

        /* Allocation table addresses */
        output->alloc_table_addr = (block0->block_size) * 16; // Allocation table address in bytes
        output->alloc_table_size = (block0->block_count * sizeof(uint64_t)); // Table size in bytes
        output->alloc_table_block = 16;
        output->alloc_table_blocks = BYTES_TO_BLOCKS(output->alloc_table_size, output->block_size);


        /* Calculate where the main directory block is */
        output->main_dir_block = output->alloc_table_blocks + 16;
        output->main_dir_blocks = block0->main_dir_blocks;

        kfree(block0);
        fd_close(device_fd);

        /* Return true */
        return 1;
    }

    kfree(block0);
    fd_close(device_fd);

    /* Return false */
    return 0;
}

/* Read a block off of an echFS drive */
void *echfs_read_block(echfs_filesystem_t *filesystem, uint64_t block) {
    void *data_area = kcalloc(filesystem->block_size);
    int device_fd = fd_open(filesystem->device_name, 0);

    // Read data
    fd_seek(device_fd, block * filesystem->block_size, 0);
    fd_read(device_fd, data_area, filesystem->block_size);

    // Close and return
    fd_close(device_fd);
    return data_area;
}

/* Read a directory entry from the main directory */
echfs_dir_entry_t *echfs_read_dir_entry(echfs_filesystem_t *filesystem, uint64_t entry) {
    echfs_dir_entry_t *data_area = kcalloc(sizeof(echfs_dir_entry_t));
    int device_fd = fd_open(filesystem->device_name, 0);

    uint64_t main_dir_start_byte = filesystem->main_dir_block * filesystem->block_size;
    fd_seek(device_fd, main_dir_start_byte + (entry * sizeof(echfs_dir_entry_t)), 0);
    fd_read(device_fd, data_area, sizeof(echfs_dir_entry_t));

    fd_close(device_fd);
    return data_area;
}

/* Get the entry in the allocation table for a block */
uint64_t echfs_get_entry_for_block(echfs_filesystem_t *filesystem, uint64_t block) {
    uint64_t alloc_table_block = ((block * 8) / filesystem->block_size) + 16;
    uint64_t *alloc_table_data = echfs_read_block(filesystem, alloc_table_block);

    uint64_t entry = alloc_table_data[block % (filesystem->block_size / 8)];

    kfree(alloc_table_data);
    return entry;
}

/* Read a file */
void *echfs_read_file(echfs_filesystem_t *filesystem, echfs_dir_entry_t *file) {
    uint8_t *data = kcalloc(ROUND_UP(file->file_size_bytes, filesystem->block_size));
    uint64_t current_block = file->starting_block;
    uint64_t byte_offset = 0;
    
    while (1) {
        /* Read the data */
        uint8_t *temp_data = echfs_read_block(filesystem, current_block);
        memcpy(temp_data, (data + byte_offset), filesystem->block_size);
        kfree(temp_data);

        /* Get new offset */
        byte_offset += filesystem->block_size;

        /* Figure out if the next block is valid */
        current_block = echfs_get_entry_for_block(filesystem, current_block);
        if (current_block == ECHFS_END_OF_CHAIN) {
            return (void *) data;
        }
    }
    
}

echfs_dir_entry_t *echfs_get_entry_from_id(echfs_filesystem_t *filesystem, uint64_t id) {
    uint64_t entry_n = 0;

    while (1) {
        echfs_dir_entry_t *entry = echfs_read_dir_entry(filesystem, entry_n);
        if (entry->entry_type == 1 && entry->starting_block == id) {
            return entry;
        }

        if (entry->parent_id == 0) {
            kfree(entry);
            return (echfs_dir_entry_t *) 0;
        }

        kfree(entry);
    }
}

char *echfs_get_full_path(echfs_filesystem_t *filesystem, char *mountpoint, echfs_dir_entry_t *entry) {
    echfs_dir_entry_t *cur_entry = entry;
    linked_strings_t *path = kcalloc(sizeof(linked_strings_t));
    while (1) {
        path->string = kcalloc(strlen(cur_entry->name) + 1);

        sprintf("\nNode: ");
        sprint(cur_entry->name);

        strcpy(cur_entry->name, path->string);
        path->next = kcalloc(sizeof(linked_strings_t));
        path->next->prev = path;
        path = path->next;

        uint64_t id = cur_entry->parent_id;
        if (cur_entry != entry) {
            kfree(cur_entry);
        }

        if (id == ECHFS_ROOT_DIR_ID) break;

        cur_entry = echfs_get_entry_from_id(filesystem, id);
    }

    /* Create final path */
    char *final_string = kcalloc(strlen(mountpoint) + 1);
    strcpy(mountpoint, final_string);

    path = path->prev;
    kfree(path->next);

    while (path) {
        final_string = krealloc(final_string, strlen(final_string) + strlen(path->string) + 1);
        path_join(final_string, path->string);

        linked_strings_t *prev = path->prev;
        kfree(path);
        kfree(path->string);
        path = prev;
    }

    return final_string;
}

vfs_node_t *echfs_create_vfs_tree(echfs_filesystem_t *filesystem, char *mountpoint_path) {
    uint64_t entry_n = 0;

    while (1) {
        echfs_dir_entry_t *entry = echfs_read_dir_entry(filesystem, entry_n);
        if (entry->parent_id == 0) {
            kfree(entry);
            break;
        }

        if (entry->parent_id == ECHFS_DELETED_ENTRY) {
            kfree(entry);
            continue;
        }

        sprintf("\nFound entry with name: ");
        sprint(entry->name);
        sprintf("\nAnd parent id %lx", entry->parent_id);

        char *full_path = echfs_get_full_path(filesystem, mountpoint_path, entry);

        sprintf("\nFull path name: ");
        sprint(full_path);

        char *output = kcalloc(201);
        char *node_name = get_path_elem(full_path, output);

        vfs_node_t *new_node = vfs_new_node(node_name, dummy_ops);
        add_node_at_path(full_path, new_node);

        sprintf("\nFull path name: ");
        sprint(full_path);
        sprintf("\nNode name: ");
        sprint(node_name);

        entry_n++;
        kfree(entry);
        kfree(node_name);
        kfree(full_path);
    }

    return (vfs_node_t *) 0;
}

void echfs_test(char *device) {
    echfs_filesystem_t filesystem;

    int is_echfs = echfs_read_block0(device, &filesystem);

    if (is_echfs) {
        sprintf("\nMain directory block: %lu", filesystem.main_dir_block);

        echfs_dir_entry_t *entry0 = echfs_read_dir_entry(&filesystem, 3);

        sprintf("\nFile name: ");
        sprintf(entry0->name);

        uint64_t start = stopwatch_start();
        char *file = echfs_read_file(&filesystem, entry0);
        uint64_t time_elapsed = stopwatch_stop(start); 

        sprintf("Size: %lu", strlen(file));

        sprintf("\nDone. Took %lu ms.", time_elapsed);

        kfree(entry0);
        kfree(file);

        echfs_create_vfs_tree(&filesystem, "/");
        sprint_all_vfs("/");
        sprint_all_vfs("/hello");
    }
}