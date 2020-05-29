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

hashmap_t *echfs_mountpoints;

/* Parse the first block of information */
int echfs_read_block0(char *device, echfs_filesystem_t *output) {
    echfs_block0_t *block0 = kcalloc(sizeof(echfs_block0_t));
    int device_fd = fd_open(device, 0);

    if (device_fd < 0) {
        kfree(block0);
        return 0;
    }

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

/* Write a block to an echFS drive */
void echfs_write_block(echfs_filesystem_t *filesystem, uint64_t block, void *data) {
    int device_fd = fd_open(filesystem->device_name, 0);

    // Read data
    fd_seek(device_fd, block * filesystem->block_size, 0);
    fd_write(device_fd, data, filesystem->block_size);

    // Close and return
    fd_close(device_fd);
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

/* Write a directory entry to the main directory */
void echfs_write_dir_entry(echfs_filesystem_t *filesystem, uint64_t entry, echfs_dir_entry_t *data) {
    int device_fd = fd_open(filesystem->device_name, 0);

    uint64_t main_dir_start_byte = filesystem->main_dir_block * filesystem->block_size;
    fd_seek(device_fd, main_dir_start_byte + (entry * sizeof(echfs_dir_entry_t)), 0);
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

    sprintf("\n[EchFS] Path: ");
    sprint(current_name);

    // Do magic lookup things

    if (!is_last) {
        /* Lookup the directory IDs */
        uint64_t found_elem_index = echfs_find_entry_name_parent(filesystem, current_name, current_parent);
        if (found_elem_index != ECHFS_SEARCH_FAIL) {
            if (cur_entry) kfree(cur_entry); // Free the old entry, if it exists
            cur_entry = echfs_read_dir_entry(filesystem, found_elem_index);
            if (!cur_entry) sprintf("\n[DripOS] Ah yes. Unreachable code. Good stuff.");

            // Found file in path, before last entry
            if (cur_entry->entry_type != 1) {
                sprint(cur_entry->name);
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

            sprintf("\n[EchFS] Resolved path.");
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

int echfs_seek(int fd_no, uint64_t offset, int whence) {
    (void) fd_no;
    (void) offset;
    (void) whence;

    return 0;
}

int echfs_read(int fd_no, void *buf, uint64_t count) {
    fd_entry_t *fd = fd_lookup(fd_no);
    vfs_node_t *node = fd->node;
    assert(node);
    char *path = get_full_path(node);
    sprintf("\n[EchFS] Full path: ");
    sprint(path);

    echfs_filesystem_t *filesystem_info = get_unid_fs_data(node->fs_root->unid);
    if (filesystem_info) {
        sprintf("\n[EchFS] Got read for mountpoint ");
        sprint(filesystem_info->mountpoint_path);
        path += strlen(filesystem_info->mountpoint_path);
        sprintf("\n[EchFS] File path: ");
        sprint(path);

        uint8_t err;
        echfs_dir_entry_t *entry = echfs_path_resolve(filesystem_info, path, &err);
        if (!entry) {
            sprintf("\n[EchFS] Read died somehow with entry getting");
            get_thread_locals()->errno = -ENOENT;

            kfree(path);
            return -1;
        }

        uint64_t read_count = 0;
        uint8_t *local_buf = echfs_read_file(filesystem_info, entry, &read_count);
        sprintf("\nBuf addr: %lx", local_buf);

        uint64_t count_to_read = count;

        if (count_to_read == 0) count_to_read = read_count;

        if (count_to_read + fd->seek > read_count) {
            get_thread_locals()->errno = -EINVAL;

            kfree(path);
            kfree(local_buf);
            kfree(entry);
            return -1;
        }

        memcpy(local_buf + fd->seek, buf, count_to_read); // Copy the data
        kfree(local_buf);
        kfree(entry);
        sprintf("\n[EchFS] Read data successfully!\n");
        return count_to_read; // Done
    } else {
        sprintf("\n[EchFS] Read died somehow\n");
        get_thread_locals()->errno = -ENOENT;

        kfree(path);
        return -1;
    }
}

/* Node generator for VFS */
void echfs_node_gen(vfs_node_t *fs_node, vfs_node_t *target_node, char *path) {
    echfs_filesystem_t *fs_data = get_unid_fs_data(fs_node->unid);

    sprintf("\n[EchFS] Searching for path %s", path);

    uint8_t err;
    echfs_dir_entry_t *entry = echfs_path_resolve(fs_data, path, &err);
    if (!entry) return;
    sprintf("\n[EchFS] Got entry for path %s", path);
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
        sprintf("\nMain directory block: %lu", filesystem->main_dir_block);

        echfs_dir_entry_t *entry0 = echfs_read_dir_entry(filesystem, 3);

        sprintf("\nFile name: ");
        sprintf(entry0->name);

        kfree(entry0);

        char *mountpoint_lol = "/echfs_mount";
        char *mountpoint_name_lol = "echfs_mount";

        filesystem->mountpoint_path = kcalloc(strlen(mountpoint_lol) + 1);

        strcpy(mountpoint_lol, filesystem->mountpoint_path);


        vfs_node_t *echfs_mountpoint = vfs_new_node(mountpoint_name_lol, dummy_ops);
        filesystem->mountpoint = echfs_mountpoint;
        
        sprintf("\nUNID for mountpoint: %lu", echfs_mountpoint->unid);
        vfs_add_child(root_node, echfs_mountpoint);

        register_unid(echfs_mountpoint->unid, filesystem);
        echfs_mountpoint->node_handle = echfs_node_gen;

        int hello_fd = fd_open("/echfs_mount/hello/README.md", 0);
        sprintf("\n[EchFS] FD: %d", hello_fd);

        char *buf = kcalloc(100);
        fd_seek(hello_fd, 0, 0);
        sprintf("\nErrno: %d", fd_read(hello_fd, buf, 0));
        sprintf("\n[EchFS] Read data: ");
        sprint(buf);
        fd_close(hello_fd);
    }
}