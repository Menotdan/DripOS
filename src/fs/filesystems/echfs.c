#include "echfs.h"
#include "fs/fd.h"
#include "fs/vfs/vfs.h"
#include "klibc/errno.h"
#include "klibc/stdlib.h"
#include "klibc/string.h"
#include "klibc/math.h"
#include "klibc/hashmap.h"
#include "klibc/strhashmap.h"
#include "fs/filesystems/filesystems.h"

#include "mm/pmm.h"

#include "drivers/pit.h"

#include "drivers/serial.h"
#include "proc/scheduler.h"

hashmap_t cached_blocks;
lock_t echfs_cache_lock = {0, 0, 0, 0};

/* Parse the first block of information */
int echfs_read_block0(char *device, echfs_filesystem_t *output) {
    echfs_block0_t *block0 = kcalloc(sizeof(echfs_block0_t));
    int device_fd = fd_open(device, 0);

    if (device_fd < 0) {
        kfree(block0);
        return 0;
    }

    fd_read(device_fd, block0, sizeof(echfs_block0_t));
    fd_seek(device_fd, 0, SEEK_SET);

    if (block0->sig[0] == '_' && block0->sig[1] == 'E' && block0->sig[2] == 'C' && block0->sig[3] == 'H' &&
        block0->sig[4] == '_' && block0->sig[5] == 'F' && block0->sig[6] == 'S' && block0->sig[7] == '_') {

        sprintf("Found echFS drive.\nBlock count: %lu, Block size: %lu\nMain dir blocks: %lu\n", block0->block_count, block0->block_size, block0->main_dir_blocks);

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

    lock(echfs_cache_lock);
    void *cached = hashmap_get_elem(&cached_blocks, block);
    if (cached) {
        memcpy(cached, data_area, filesystem->block_size);
        unlock(echfs_cache_lock);
        return data_area;
    }
    unlock(echfs_cache_lock);

    int device_fd = fd_open(filesystem->device_name, 0);

    // Read data
    fd_seek(device_fd, block * filesystem->block_size, SEEK_SET);
    fd_read(device_fd, data_area, filesystem->block_size);

    lock(echfs_cache_lock);
    void *cached_block = kcalloc(filesystem->block_size);
    memcpy(data_area, cached_block, filesystem->block_size);
    hashmap_set_elem(&cached_blocks, block, cached_block);
    unlock(echfs_cache_lock);

    // Close and return
    fd_close(device_fd);
    return data_area;
}

/* Write a block to an echFS drive */
void echfs_write_block(echfs_filesystem_t *filesystem, uint64_t block, void *data) {
    int device_fd = fd_open(filesystem->device_name, 0);

    // Read data
    fd_seek(device_fd, block * filesystem->block_size, SEEK_SET);
    fd_write(device_fd, data, filesystem->block_size);

    lock(echfs_cache_lock);
    void *cached = hashmap_get_elem(&cached_blocks, block);
    if (cached) {
        memcpy(data, cached, filesystem->block_size);
    }
    unlock(echfs_cache_lock);

    // Close and return
    fd_close(device_fd);
}

/* Read a directory entry from the main directory */
echfs_dir_entry_t *echfs_read_dir_entry(echfs_filesystem_t *filesystem, uint64_t entry) {
    echfs_dir_entry_t *data_area = kcalloc(sizeof(echfs_dir_entry_t));
    int device_fd = fd_open(filesystem->device_name, 0);

    uint64_t main_dir_start_byte = filesystem->main_dir_block * filesystem->block_size;
    fd_seek(device_fd, main_dir_start_byte + (entry * sizeof(echfs_dir_entry_t)), SEEK_SET);
    fd_read(device_fd, data_area, sizeof(echfs_dir_entry_t));

    fd_close(device_fd);
    return data_area;
}

/* Write a directory entry to the main directory */
void echfs_write_dir_entry(echfs_filesystem_t *filesystem, uint64_t entry, echfs_dir_entry_t *data) {
    int device_fd = fd_open(filesystem->device_name, 0);

    uint64_t main_dir_start_byte = filesystem->main_dir_block * filesystem->block_size;
    fd_seek(device_fd, main_dir_start_byte + (entry * sizeof(echfs_dir_entry_t)), SEEK_SET);
    fd_write(device_fd, data, sizeof(echfs_dir_entry_t));

    fd_close(device_fd);
}

/* Get the entry in the allocation table for a block */
uint64_t echfs_get_entry_for_block(echfs_filesystem_t *filesystem, uint64_t block) {
    uint64_t alloc_table_block = ((block * 8) / filesystem->block_size) + 16;
    uint64_t *alloc_table_data = echfs_read_block(filesystem, alloc_table_block);

    uint64_t entry = alloc_table_data[block % (filesystem->block_size / 8)];

    kfree(alloc_table_data);
    return entry;
}

/* Get the entry in the allocation table for a block */
void echfs_set_entry_for_block(echfs_filesystem_t *filesystem, uint64_t block, uint64_t data) {
    uint64_t alloc_table_block = ((block * 8) / filesystem->block_size) + 16;

    uint64_t *alloc_table_data = echfs_read_block(filesystem, alloc_table_block);
    alloc_table_data[block % (filesystem->block_size / 8)] = data;
    echfs_write_block(filesystem, alloc_table_block, alloc_table_data);

    kfree(alloc_table_data);
}

/* Read a file */
void *echfs_read_file(echfs_filesystem_t *filesystem, echfs_dir_entry_t *file, uint64_t *read_count) {
    uint8_t *data = kcalloc(ROUND_UP(file->file_size_bytes, filesystem->block_size));
    uint64_t current_block = file->starting_block;
    uint64_t byte_offset = 0;
    *read_count = file->file_size_bytes;
    
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

uint64_t echfs_find_entry_name_parent(echfs_filesystem_t *filesystem, char *name, uint64_t parent_id) {
    uint64_t entry_n = 0;

    while (1) {
        echfs_dir_entry_t *entry = echfs_read_dir_entry(filesystem, entry_n++);
        if (entry->parent_id == parent_id && strcmp(name, entry->name) == 0) {
            kfree(entry);
            return entry_n - 1;
        }

        if (entry->parent_id == 0) {
            kfree(entry);
            return ECHFS_SEARCH_FAIL; // ERR
        }

        kfree(entry);
    }
}

echfs_dir_entry_t *echfs_path_resolve(echfs_filesystem_t *filesystem, char *filename, uint8_t *err_code) {
    char current_name[201] = {'\0'}; // Filenames are max 201 chars
    uint8_t is_last = 0; // Is this the last in the path?
    *err_code = 0;
    echfs_dir_entry_t *cur_entry = (echfs_dir_entry_t *) 0;
    uint64_t current_parent = ECHFS_ROOT_DIR_ID;

    (void) filesystem;

    if (*filename == '/') filename++; // Ignore the first '/' to make parsing the path easier

    if (strcmp(filename, "/") == 0) {
        *err_code |= (1<<0); // Root entry
        return (echfs_dir_entry_t *) 0; // Fail
    }

    // For every part of the path
next_elem:
    // Store the name of the current element in the path
    current_name[0] = '\0';

    for (uint64_t i = 0; *filename != '/'; filename++) {
        if (*filename == '\0') {
            is_last = 1;
            break;
        }

        current_name[i++] = *filename;
        current_name[i] = '\0';

        // Name too long handling
        if (i == 201 && *(filename + 1) != '\0') {
            if (cur_entry) kfree(cur_entry);
            *err_code |= (1<<1); // Name too long
            return (echfs_dir_entry_t *) 0;
        }
    }
    filename++; // Get rid of the final '/'

    //sprintf("[EchFS] Path: %s\n", current_name);

    /* Do magic lookup things */
    if (!is_last) {
        /* Lookup the directory IDs */
        uint64_t found_elem_index = echfs_find_entry_name_parent(filesystem, current_name, current_parent);
        if (found_elem_index != ECHFS_SEARCH_FAIL) {
            if (cur_entry) kfree(cur_entry); // Free the old entry, if it exists
            cur_entry = echfs_read_dir_entry(filesystem, found_elem_index);
            if (!cur_entry) sprintf("[DripOS] ok then\n");

            // Found file in path, before last entry
            if (cur_entry->entry_type != 1) {
                //sprintf("%s\n", cur_entry->name);
                if (cur_entry) kfree(cur_entry);
                *err_code |= (1<<2); // Search failed
                return (echfs_dir_entry_t *) 0;
            }
            current_parent = cur_entry->starting_block;
        } else {
            if (cur_entry) kfree(cur_entry);
            *err_code |= (1<<2); // Search failed
            return (echfs_dir_entry_t *) 0;
        }
        goto next_elem;
    } else {
        uint64_t found_elem_index = echfs_find_entry_name_parent(filesystem, current_name, current_parent);
        if (found_elem_index != ECHFS_SEARCH_FAIL) {
            if (cur_entry) kfree(cur_entry); // Free the old entry, if it exists
            cur_entry = echfs_read_dir_entry(filesystem, found_elem_index);

            //sprintf("[EchFS] Resolved path.\n");
            return cur_entry;
        } else {
            if (cur_entry) kfree(cur_entry);
            *err_code |= (1<<2); // Search failed
            return (echfs_dir_entry_t *) 0;
        }
    }

    return (echfs_dir_entry_t *) 0;
}

/* EchFS VFS ops */
int echfs_open(char *name, int mode) {
    (void) name;
    (void) mode;

    return 0;
}

int echfs_close(int fd_no) {
    (void) fd_no;

    return 0;
}

uint64_t echfs_seek(int fd_no, uint64_t offset, int whence) {
    (void) offset;

    if (whence == SEEK_END) {
        fd_entry_t *fd = fd_lookup(fd_no);
        echfs_filesystem_t *filesystem_info = get_unid_fs_data(fd->node->fs_root->unid);
        if (!filesystem_info) {
            return -ENOENT; // idk lol
        }

        char *path = get_full_path(fd->node);
        path += strlen(filesystem_info->mountpoint_path);

        uint8_t err;
        echfs_dir_entry_t *entry = echfs_path_resolve(filesystem_info, path, &err);
        if (!entry) {
            kfree(path);
            return -ENOENT; // somehow
        }

        fd->seek = entry->file_size_bytes - 1; // ok done lol
        kfree(entry);
        kfree(path);
    }

    return 0;
}

void *read_blocks_for_range(echfs_filesystem_t *filesystem, echfs_dir_entry_t *file, uint64_t read_start, uint64_t read_count) {
    uint64_t start = ROUND_DOWN(read_start, filesystem->block_size);
    uint64_t end = ROUND_UP(read_start + read_count, filesystem->block_size);

    uint64_t blocks_to_read = (end - start) / filesystem->block_size;
    uint64_t start_block = start / filesystem->block_size;
    uint64_t current_block = file->starting_block;
    uint8_t *block_buffer = kcalloc(blocks_to_read * filesystem->block_size);
    uint8_t *current_blockbuf_pointer = block_buffer;

    for (uint64_t i = 0; i < start_block; i++) {
        current_block = echfs_get_entry_for_block(filesystem, current_block);
        //sprintf("skipping to block: %lx\n", current_block);
        if (current_block == ECHFS_END_OF_CHAIN) {
            kfree(block_buffer);
            sprintf("failed to get to the correct start block\n");
            return (void *) 0;
        }
    }

    for (uint64_t i = 0; i < blocks_to_read; i++) {
        void *block = echfs_read_block(filesystem, current_block);
        memcpy(block, current_blockbuf_pointer, filesystem->block_size);
        current_blockbuf_pointer += filesystem->block_size;
        kfree(block);

        if (i == blocks_to_read - 1) {
            break;
        }

        current_block = echfs_get_entry_for_block(filesystem, current_block);
        if (current_block == ECHFS_END_OF_CHAIN) {
            kfree(block_buffer);
            sprintf("failed to get to the next block\n");
            return (void *) 0;
        }
    }

    return block_buffer;
}

void *read_for_range(echfs_filesystem_t *filesystem, echfs_dir_entry_t *file, uint64_t read_start, uint64_t read_count) {
    uint8_t *block_buffer = read_blocks_for_range(filesystem, file, read_start, read_count);
    uint8_t *output_buffer = kcalloc(read_count);
    block_buffer += read_start % filesystem->block_size;

    memcpy(block_buffer, output_buffer, read_count);
    block_buffer -= read_start % filesystem->block_size;
    kfree(block_buffer);
    return output_buffer;
}

int echfs_read(int fd_no, void *buf, uint64_t count) {
    fd_entry_t *fd = fd_lookup(fd_no);
    vfs_node_t *node = fd->node;
    assert(node);
    char *path = get_full_path(node);
    char *original_path_addr = path;
    //sprintf("[EchFS] Full path: %s\n", path);

    echfs_filesystem_t *filesystem_info = get_unid_fs_data(node->fs_root->unid);
    if (filesystem_info) {
        //sprintf("[EchFS] Got read for mountpoint %s\n", filesystem_info->mountpoint_path);
        path += strlen(filesystem_info->mountpoint_path);
        //sprintf("[EchFS] File path: %s\n", path);

        uint8_t err;
        echfs_dir_entry_t *entry = echfs_path_resolve(filesystem_info, path, &err);
        if (!entry) {
            sprintf("[EchFS] Read died somehow with entry getting\n");

            kfree(original_path_addr);
            return -ENOENT;
        }

        uint64_t count_to_read = count;
        if (!count_to_read) {
            sprintf("count_to_read is null\n");
            kfree(original_path_addr);
            kfree(entry);
            return 0;
        }
        if (count_to_read + fd->seek > entry->file_size_bytes) {
            sprintf("count_to_read bad\n");
            sprintf("count_to_read: %lu, fd->seek: %lu, entry->file_size_bytes: %lu\n", count_to_read, fd->seek, entry->file_size_bytes);
            kfree(original_path_addr);
            kfree(entry);
            return -EINVAL;
        }
        uint8_t *local_buf = read_for_range(filesystem_info, entry, fd->seek, count_to_read);

        memcpy(local_buf, buf, count_to_read); // Copy the data
        kfree(local_buf);
        kfree(entry);
        kfree(original_path_addr);
        fd->seek += count_to_read;
        //sprintf("[EchFS] Read data successfully!\n");
        return count_to_read; // Done
    } else {
        sprintf("[EchFS] Read died somehow\n");

        kfree(path);
        return -ENOENT;
    }
}

/* Node generator for VFS */
void echfs_node_gen(vfs_node_t *fs_node, vfs_node_t *target_node, char *path) {
    echfs_filesystem_t *fs_data = get_unid_fs_data(fs_node->unid);

    //sprintf("[EchFS] Searching for path %s\n", path);

    uint8_t err;
    echfs_dir_entry_t *entry = echfs_path_resolve(fs_data, path, &err);
    if (!entry) return;
    //sprintf("[EchFS] Got entry for path %s\n", path);
    kfree(entry);

    vfs_ops_t echfs_ops = {echfs_open, echfs_close, echfs_read, 0, echfs_seek};

    /* Set all the VFS ops */
    set_child_ops(target_node, echfs_ops);
    target_node->ops = echfs_ops;
    set_child_fs_root(target_node, fs_node);
    target_node->fs_root = fs_node;
}

void echfs_test(char *device) {
    echfs_filesystem_t *filesystem = kcalloc(sizeof(echfs_filesystem_t));

    int is_echfs = echfs_read_block0(device, filesystem);

    if (is_echfs) {
        sprintf("Main directory block: %lu\n", filesystem->main_dir_block);

        echfs_dir_entry_t *entry0 = echfs_read_dir_entry(filesystem, 3);

        sprintf("File name: \n");
        sprintf(entry0->name);

        kfree(entry0);

        char *mountpoint_lol = "/";
        filesystem->mountpoint_path = kcalloc(strlen(mountpoint_lol) + 1);
        strcpy(mountpoint_lol, filesystem->mountpoint_path);

        filesystem->mountpoint = root_node;

        register_unid(root_node->unid, filesystem);
        root_node->node_handle = echfs_node_gen;

        int hello_fd = fd_open("/file_read_test", 0);
        sprintf("[EchFS] FD: %d\n", hello_fd);

        char *buf = kcalloc(0x10000);
        fd_seek(hello_fd, 3600, SEEK_SET);
        sprintf("Errno: %d\n", fd_read(hello_fd, buf, 100));
        sprintf("[EchFS] Read data: \n");
        sprintf("%s\n", buf);
        fd_close(hello_fd);
    }
}