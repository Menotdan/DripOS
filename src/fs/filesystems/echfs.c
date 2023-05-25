#include "echfs.h"
#include "fs/fd.h"
#include "fs/vfs/vfs.h"
#include "klibc/errno.h"
#include "klibc/stdlib.h"
#include "klibc/string.h"
#include "klibc/math.h"
#include "klibc/hashmap.h"
#include "klibc/strhashmap.h"
#include "klibc/open_flags.h"
#include "klibc/logger.h"
#include "fs/filesystems/filesystems.h"

#include "mm/pmm.h"

#include "drivers/pit.h"
#include "drivers/rtc.h"

#include "drivers/serial.h"
#include "proc/scheduler.h"

hashmap_t cached_blocks;
lock_t echfs_cache_lock = {0, 0, 0, 0};

int echfs_open(char *name, int mode);
int echfs_post_open(int fd, int mode);
int echfs_close(int fd_no);
int echfs_read(int fd_no, void *buf, uint64_t count);
int echfs_write(int fd_no, void *buf, uint64_t count);
uint64_t echfs_seek(int fd_no, uint64_t offset, int whence);

vfs_ops_t echfs_ops = {echfs_open, echfs_post_open, echfs_close, echfs_read, echfs_write, echfs_seek};

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
    echfs_dir_entry_t *data_area = kcalloc(sizeof(echfs_dir_entry_t) - 8);
    int device_fd = fd_open(filesystem->device_name, 0);

    uint64_t main_dir_start_byte = filesystem->main_dir_block * filesystem->block_size;
    fd_seek(device_fd, main_dir_start_byte + (entry * (sizeof(echfs_dir_entry_t) - 8)), SEEK_SET);
    fd_read(device_fd, data_area, sizeof(echfs_dir_entry_t) - 8);

    fd_close(device_fd);
    return data_area;
}

/* Write a directory entry to the main directory */
void echfs_write_dir_entry(echfs_filesystem_t *filesystem, uint64_t entry, echfs_dir_entry_t *data) {
    int device_fd = fd_open(filesystem->device_name, 0);

    uint64_t main_dir_start_byte = filesystem->main_dir_block * filesystem->block_size;
    fd_seek(device_fd, main_dir_start_byte + (entry * (sizeof(echfs_dir_entry_t) - 8)), SEEK_SET);
    fd_write(device_fd, data, sizeof(echfs_dir_entry_t) - 8);

    fd_close(device_fd);
}

uint64_t get_free_directory_entry(echfs_filesystem_t *filesystem) {
    for (uint64_t i = 0; i < (filesystem->main_dir_blocks * filesystem->block_size) / (sizeof(echfs_dir_entry_t) - 8); i++) {
        echfs_dir_entry_t *dir_entry = echfs_read_dir_entry(filesystem, i);

        uint64_t id = dir_entry->parent_id;
        kfree(dir_entry);

        if (id == 0 || id == ECHFS_DELETED_ENTRY) {
            return i;
        }
    }
    
    return 0;
}

/* Get the entry in the allocation table for a block */
uint64_t echfs_get_entry_for_block(echfs_filesystem_t *filesystem, uint64_t block) {
    uint64_t alloc_table_block = ((block * 8) / filesystem->block_size) + 16;
    uint64_t *alloc_table_data = echfs_read_block(filesystem, alloc_table_block);

    uint64_t entry = alloc_table_data[block % (filesystem->block_size / 8)];

    kfree(alloc_table_data);
    return entry;
}

/* Set the entry in the allocation table for a block */
void echfs_set_entry_for_block(echfs_filesystem_t *filesystem, uint64_t block, uint64_t data) {
    uint64_t alloc_table_block = ((block * 8) / filesystem->block_size) + 16;

    uint64_t *alloc_table_data = echfs_read_block(filesystem, alloc_table_block);
    alloc_table_data[block % (filesystem->block_size / 8)] = data;
    echfs_write_block(filesystem, alloc_table_block, alloc_table_data);

    kfree(alloc_table_data);
}

uint64_t find_free_block(echfs_filesystem_t *filesystem) {
    for (uint64_t i = 0; i < filesystem->blocks; i++) {
        if (!echfs_get_entry_for_block(filesystem, i)) {
            return i;
        }
    }

    return 0;
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

echfs_dir_entry_t *echfs_path_resolve(echfs_filesystem_t *filesystem, char *filename, uint8_t *err_code, uint64_t unid) {
    (void) unid;

    char current_name[201] = {'\0'}; // Filenames are max 201 chars
    uint8_t is_last = 0; // Is this the last in the path?
    *err_code = 0;
    echfs_dir_entry_t *cur_entry = (echfs_dir_entry_t *) 0;
    uint64_t current_parent = ECHFS_ROOT_DIR_ID;

    if (*filename == '/') filename++; // Ignore the first '/' to make parsing the path easier

    if (strcmp(filename, "/") == 0) {
        *err_code |= (1<<0); // Root entry
        sprintf("found root entry ;-; %s\n", filename);
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
            sprintf("name too long, name %s\n", current_name);
            return (echfs_dir_entry_t *) 0;
        }
    }
    filename++; // Get rid of the final '/'

    /* Do magic lookup things */
    if (!is_last) {
        /* Lookup the directory IDs */
        uint64_t found_elem_index = echfs_find_entry_name_parent(filesystem, current_name, current_parent);
        if (found_elem_index != ECHFS_SEARCH_FAIL) {
            if (cur_entry) kfree(cur_entry); // Free the old entry, if it exists
            cur_entry = echfs_read_dir_entry(filesystem, found_elem_index);
            if (!cur_entry) sprintf("[DripOS] ok then\n");

            // Found file in path, before last entry
            if (cur_entry->entry_type != ECHFS_TYPE_DIR) {
                sprintf("found file %s in area where a directory is supposed to be\n", cur_entry->name);

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

            cur_entry->entry_number = found_elem_index;

            return cur_entry;
        } else {
            if (cur_entry) kfree(cur_entry);
            *err_code |= (1<<2); // Search failed
            return (echfs_dir_entry_t *) 0;
        }
    }

    return (echfs_dir_entry_t *) 0;
}

void allocate_blocks_for_file(echfs_filesystem_t *filesystem, echfs_dir_entry_t *file, uint64_t total_blocks) {
    uint64_t blocks_total = 0;
    uint64_t cur_block = file->starting_block;

    while (1) {
        blocks_total += 1;
        if (echfs_get_entry_for_block(filesystem, cur_block) == ECHFS_END_OF_CHAIN) {
            break;
        }
        cur_block = echfs_get_entry_for_block(filesystem, cur_block);
    }

    for (uint64_t i = 0; i < (total_blocks - blocks_total); i++) {
        uint64_t free_block = find_free_block(filesystem);
        echfs_set_entry_for_block(filesystem, cur_block, free_block);
        cur_block = free_block;
        echfs_set_entry_for_block(filesystem, cur_block, ECHFS_END_OF_CHAIN);
    }
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
        char *path_ptr = path; // Can't free at a misaligned address
        path_ptr += strlen(filesystem_info->mountpoint_path);

        uint8_t err;
        echfs_dir_entry_t *entry = echfs_path_resolve(filesystem_info, path_ptr, &err, fd->node->unid);
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
    if (!block_buffer) {
        return (void *) 0;
    }

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
    char *original_path = get_full_path(node);
    char *path = get_mountpoint_relative_path(node->fs_root, original_path);
    path[strlen(path) - 1] = '\0'; // Remove trailing /

    echfs_filesystem_t *filesystem_info = get_unid_fs_data(node->fs_root->unid);
    if (!filesystem_info) {
        sprintf("[EchFS] Read died somehow\n");

        kfree(path);
        kfree(original_path);
        return -ENOENT;
    }

    uint8_t err;
    echfs_dir_entry_t *entry = echfs_path_resolve(filesystem_info, path, &err, fd->node->unid);
    if (!entry) {
        sprintf("Name: %s, Cur Name: %s\n", original_path, path);
        sprintf("[EchFS] Read died somehow with entry getting\n");

        kfree(path);
        kfree(original_path);
        return -ENOENT;
    }

    uint64_t count_to_read = count;
    if (!count_to_read) {
        sprintf("count_to_read is null\n");

        kfree(path);
        kfree(original_path);
        kfree(entry);
        return 0;
    }

    if (count_to_read + fd->seek > entry->file_size_bytes) {
        sprintf("count_to_read bad\n");
        sprintf("count_to_read: %lu, fd->seek: %lu, entry->file_size_bytes: %lu\n", count_to_read, fd->seek, entry->file_size_bytes);

        kfree(path);
        kfree(original_path);
        kfree(entry);
        return -EINVAL;
    }

    uint8_t *local_buf = read_for_range(filesystem_info, entry, fd->seek, count_to_read);
    if (!local_buf) {
        kfree(path);
        kfree(entry);
        kfree(original_path);
        return -EIO;
    }

    memcpy(local_buf, buf, count_to_read); // Copy the data
    kfree(local_buf);
    kfree(entry);
    kfree(original_path);
    kfree(path);
    fd->seek += count_to_read;

    return count_to_read; // Done
}

int write_blocks_for_range(echfs_filesystem_t *filesystem, echfs_dir_entry_t *file, uint64_t write_start, uint64_t write_count, void *data) {
    uint64_t start = ROUND_DOWN(write_start, filesystem->block_size);
    uint64_t end = ROUND_UP(write_start + write_count, filesystem->block_size);

    uint64_t blocks_to_read = (end - start) / filesystem->block_size;
    uint64_t start_block = start / filesystem->block_size;
    uint64_t current_block = file->starting_block;
    uint8_t *cur_dat_ptr = data;

    for (uint64_t i = 0; i < start_block; i++) {
        current_block = echfs_get_entry_for_block(filesystem, current_block);
        if (current_block == ECHFS_END_OF_CHAIN) {
            sprintf("failed to get to the correct start block\n");
            return 1;
        }
    }

    for (uint64_t i = 0; i < blocks_to_read; i++) {
        echfs_write_block(filesystem, current_block, cur_dat_ptr);
        cur_dat_ptr += filesystem->block_size;

        if (i == blocks_to_read - 1) {
            break;
        }

        current_block = echfs_get_entry_for_block(filesystem, current_block);
        if (current_block == ECHFS_END_OF_CHAIN) {
            sprintf("failed to get to the next block\n");
            return 1;
        }
    }

    return 0;
}

int write_for_range(echfs_filesystem_t *filesystem, echfs_dir_entry_t *file, uint64_t write_start, uint64_t write_count, void *data) {
    uint8_t *block_buffer = read_blocks_for_range(filesystem, file, write_start, write_count);
    if (!block_buffer) {
        return 1;
    }

    block_buffer += write_start % filesystem->block_size;
    memcpy(data, block_buffer, write_count);
    block_buffer -= write_start % filesystem->block_size;

    int err = write_blocks_for_range(filesystem, file, write_start, write_count, block_buffer);
    if (err) {
        return 1;
    }

    kfree(block_buffer);
    return 0;
}

int echfs_write(int fd_no, void *buf, uint64_t count) {
    fd_entry_t *fd = fd_lookup(fd_no);
    vfs_node_t *node = fd->node;
    assert(node);
    char *path = get_full_path(node);
    char *original_path_addr = path;

    echfs_filesystem_t *filesystem_info = get_unid_fs_data(node->fs_root->unid);
    if (filesystem_info) {
        path += strlen(filesystem_info->mountpoint_path);

        uint8_t err;
        echfs_dir_entry_t *entry = echfs_path_resolve(filesystem_info, path, &err, fd->node->unid);
        if (!entry) {
            sprintf("[EchFS] Write died somehow with entry getting\n");

            kfree(original_path_addr);
            return -ENOENT;
        }

        uint64_t count_to_write = count;
        if (!count_to_write) {
            sprintf("count_to_write is null\n");
            kfree(original_path_addr);
            kfree(entry);
            return 0;
        }

        uint64_t allocated_bytes_needed = count + fd->seek;
        uint64_t allocated_blocks_needed = (allocated_bytes_needed + filesystem_info->block_size - 1) / filesystem_info->block_size;
        allocate_blocks_for_file(filesystem_info, entry, allocated_blocks_needed);
        int err_write = write_blocks_for_range(filesystem_info, entry, fd->seek, count_to_write, buf);
        if (err_write) {
            kfree(original_path_addr);
            kfree(entry);
            return -EIO;
        }
        entry->file_size_bytes = allocated_bytes_needed;
        entry->unix_modify_time = get_time_since_epoch();
        echfs_write_dir_entry(filesystem_info, entry->entry_number, entry);
        kfree(original_path_addr);
        kfree(entry);
        return count_to_write;
    } else {
        sprintf("There has been a death\n");
        kfree(original_path_addr);
        return -EIO;
    }
}

int echfs_post_open(int fd, int mode) {
    fd_entry_t *fd_dat = fd_lookup(fd);
    vfs_node_t *node = fd_dat->node;
    assert(node);
    char *path = get_full_path(node);
    char *original_path_addr = path;

    echfs_filesystem_t *filesystem_info = get_unid_fs_data(node->fs_root->unid);
    if (filesystem_info) {
        path += strlen(filesystem_info->mountpoint_path);

        uint8_t err;
        echfs_dir_entry_t *entry = echfs_path_resolve(filesystem_info, path, &err, node->unid);
        if (!entry) {
            sprintf("[EchFS] Post open died somehow with entry getting\n");

            kfree(original_path_addr);
            return -ENOENT;
        }
        
        entry->unix_access_time = get_time_since_epoch();
        echfs_write_dir_entry(filesystem_info, entry->entry_number, entry);

        if (mode & O_TRUNC) {
            sprintf("file cleared!\n");
            entry->file_size_bytes = 0;
            echfs_write_dir_entry(filesystem_info, entry->entry_number, entry);
        }

        kfree(original_path_addr);
        kfree(entry);
        return 0;
    } else {
        kfree(original_path_addr);
        return 1;
    }
}

/* Node generator for VFS */
void echfs_node_gen(vfs_node_t *fs_node, vfs_node_t *target_node, char *path) {
    echfs_filesystem_t *fs_data = get_unid_fs_data(fs_node->unid);

    sprintf("[EchFS] Searching for path %s\n", path);

    uint8_t err;
    echfs_dir_entry_t *entry = echfs_path_resolve(fs_data, path, &err, 0);
    sprintf("Found Entry in node_gen(): %lx\n\n", entry);
    if (!entry) return;
    log("[EchFS] Got entry for path %s", path);
    kfree(entry);

    /* Set all the VFS ops */
    set_child_ops(target_node, echfs_ops);
    target_node->ops = echfs_ops;
    set_child_fs_root(target_node, fs_node);
    target_node->fs_root = fs_node;
}

int echfs_create_handler(vfs_node_t *self, char *name, int mode) {
    sprintf("trying to create a file with name %s and mode %d\n", name, mode);
    echfs_filesystem_t *fs_data = get_unid_fs_data(self->unid);

    uint8_t resolve_error1;
    echfs_dir_entry_t *file = echfs_path_resolve(fs_data, name, &resolve_error1, 0);
    if (file) {
        kfree(file);
        return -EEXIST;
    }

    uint64_t parent_id = 0;

    char *editable_path = kcalloc(strlen(name) + 1);
    strcpy(name, editable_path);
    path_remove_elem(editable_path);

    if (strcmp(editable_path, "/")) {
        echfs_dir_entry_t *parent_dir = echfs_path_resolve(fs_data, editable_path, &resolve_error1, 0);
        if (!parent_dir) {
            sprintf("parent not found (%s)\n", editable_path);
            kfree(editable_path);
            return -ENOENT;
        }
        
        if (parent_dir->entry_type != ECHFS_TYPE_DIR) {
            sprintf("parent is not a directory (%s)\n", editable_path);
            kfree(editable_path);
            kfree(parent_dir);
            return -ENOTDIR;
        }

        parent_id = parent_dir->starting_block;
        kfree(parent_dir);
    } else {
        parent_id = ECHFS_ROOT_DIR_ID;
    }

    echfs_dir_entry_t new_file;

    strcpy(ptr_to_end_path_elem(name), new_file.name);
    new_file.parent_id = parent_id;
    new_file.group_id = 0;
    new_file.owner_id = 0;
    new_file.file_size_bytes = 0;
    new_file.permissions = 0;
    new_file.unix_create_time = get_time_since_epoch();
    new_file.unix_modify_time = get_time_since_epoch();
    new_file.unix_access_time = get_time_since_epoch();
    new_file.starting_block = find_free_block(fs_data);
    echfs_set_entry_for_block(fs_data, new_file.starting_block, ECHFS_END_OF_CHAIN);
    new_file.entry_type = ECHFS_TYPE_FILE;

    uint64_t free_id = get_free_directory_entry(fs_data);
    echfs_write_dir_entry(fs_data, free_id, &new_file);

    kfree(editable_path);
    return 0;
}

int echfs_file_exists(vfs_node_t *fs_root, char *path) {
    echfs_filesystem_t *fs_data = get_unid_fs_data(fs_root->unid);

    uint8_t err = 0;
    echfs_dir_entry_t *dir_entry = echfs_path_resolve(fs_data, path, &err, 0);
    if (dir_entry) {
        kfree(dir_entry);
        return 1;
    }

    return 0;
}

void echfs_test(char *device) {
    echfs_filesystem_t *filesystem = kcalloc(sizeof(echfs_filesystem_t));

    int is_echfs = echfs_read_block0(device, filesystem);

    if (is_echfs) {
        sprintf("Main directory block: %lu\n", filesystem->main_dir_block);

        char *mountpoint_lol = "/";
        filesystem->mountpoint_path = kcalloc(strlen(mountpoint_lol) + 1);
        strcpy(mountpoint_lol, filesystem->mountpoint_path);

        filesystem->mountpoint = root_node;

        register_unid(root_node->unid, filesystem);
        root_node->filesystem_descriptor = kcalloc(sizeof(vfs_fs_descriptor_t));
        root_node->filesystem_descriptor->file_exists = echfs_file_exists;
        root_node->filesystem_descriptor->fs_ops = echfs_ops;
        root_node->filesystem_descriptor->fs_root = root_node;
        root_node->node_handle = echfs_node_gen;
        root_node->create_handle = echfs_create_handler;


        int hello_fd = fd_open("/file_read_test", 0);
        sprintf("[EchFS] FD: %d\n", hello_fd);

        char *buf = kcalloc(0x10000);
        fd_seek(hello_fd, 3600, SEEK_SET);
        sprintf("Errno: %d\n", fd_read(hello_fd, buf, 100));
        sprintf("[EchFS] Read data: \n");
        sprintf("%s\n", buf);
        fd_close(hello_fd);

        int lol_fd = fd_open("/usr/lol.txt", O_CREAT);
        sprintf("opening and creating /usr/lol.txt: %d\n", lol_fd);
        if (lol_fd >= 0) {
            char *lol_dat = kcalloc(4096);
            sprintf("trying to read data\n");
            fd_write(lol_fd, "hi", 2);
            fd_read(lol_fd, lol_dat, 2);
            sprintf("done\n");
            sprintf("lol.txt data: %s\n", lol_dat);
        }
    }
}