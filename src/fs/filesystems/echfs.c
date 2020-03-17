#include "echfs.h"
#include "fs/fd.h"
#include "klibc/stdlib.h"
#include "klibc/string.h"

#include "drivers/serial.h"

int read_block0(char *device, echfs_filesystem_t *output) {
    echfs_block0_t *block0 = kcalloc(sizeof(echfs_block0_t));
    int device_fd = fd_open(device, 0);
    fd_read(device_fd, block0, sizeof(echfs_block0_t));
    fd_seek(device_fd, 0, 0);

    if (block0->sig[0] == '_' && block0->sig[1] == 'E' && block0->sig[2] == 'C' && block0->sig[3] == 'H' &&
        block0->sig[4] == '_' && block0->sig[5] == 'F' && block0->sig[6] == 'S' && block0->sig[7] == '_') {

        sprintf("\nFound echFS drive.\nBlock count: %lu, Block size: %lu\nMain dir blocks: %lu", block0->block_count, block0->block_size, block0->main_dir_blocks);

        // Set data for the filesystem structure
        output->device_name = kcalloc(strlen(device) + 1);
        strcpy(device, output->device_name); // Name
        output->blocks = block0->block_count; // Block count
        output->block_size = block0->block_size; // Block size
        output->alloc_table_addr = (block0->block_size) * 16; // Allocation table address in bytes
        output->alloc_table_size = (block0->block_count * sizeof(uint64_t)); // Table size in bytes

        // Calculate where the main directory block is
        output->main_dir_block = BYTES_TO_BLOCKS(output->alloc_table_size, block0->block_size) + 16;
        output->main_dir_blocks = block0->main_dir_blocks;

        fd_close(device_fd);

        // Return
        return 1;
    }

    fd_close(device_fd);
    return 0;
}

void *read_block(echfs_filesystem_t *filesystem, uint64_t block) {
    void *data_area = kcalloc(filesystem->block_size);
    int device_fd = fd_open(filesystem->device_name, 0);

    // Read data
    fd_seek(device_fd, block * filesystem->block_size, 0);
    fd_read(device_fd, data_area, filesystem->block_size);

    // Close and return
    fd_close(device_fd);
    return data_area;
}

echfs_dir_entry_t *read_dir_entry(echfs_filesystem_t *filesystem, uint64_t entry) {
    echfs_dir_entry_t *data_area = kcalloc(sizeof(echfs_dir_entry_t));
    int device_fd = fd_open(filesystem->device_name, 0);

    uint64_t main_dir_start_byte = filesystem->main_dir_block * filesystem->block_size;
    fd_seek(device_fd, main_dir_start_byte + (entry * sizeof(echfs_dir_entry_t)), 0);
    fd_read(device_fd, data_area, sizeof(echfs_dir_entry_t));

    fd_close(device_fd);
    return data_area;
}

void echfs_test(char *device) {
    echfs_filesystem_t filesystem;
    int is_echfs = read_block0(device, &filesystem);

    if (is_echfs) {
        sprintf("\nMain directory block: %lu", filesystem.main_dir_block);
        uint64_t *alloc_table_block0 = read_block(&filesystem, filesystem.alloc_table_addr / filesystem.block_size);
        
        echfs_dir_entry_t *entry0 = read_dir_entry(&filesystem, 2);
        sprintf("\nFirst entry name:");
        sprintf(entry0->name);
        sprintf("\ntype: %u", entry0->entry_type);
        sprintf("\nCreation time: %lu", entry0->unix_create_time);
    }
}