#include "echfs.h"
#include "fs/fd.h"
#include "klibc/stdlib.h"
#include "klibc/string.h"
#include "klibc/math.h"

#include "mm/pmm.h"

#include "drivers/pit.h"

#include "drivers/serial.h"
#include "proc/scheduler.h"

/* Parse the first block of information */
int echfs_read_block0(char *device, echfs_filesystem_t *output) {
    echfs_block0_t *block0 = kcalloc(sizeof(echfs_block0_t));
    int device_fd = fd_open(device, 0);
    uint64_t before = pmm_get_used_mem();
    fd_read(device_fd, block0, sizeof(echfs_block0_t));
    uint64_t after = pmm_get_used_mem();
    sprintf("\n%lx leaked (%lx to %lx) [fd_read]", after - before, before, after);
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

void echfs_test(char *device) {
    echfs_filesystem_t filesystem;
    uint64_t before, after;
    
    before = pmm_get_used_mem();
    int is_echfs = echfs_read_block0(device, &filesystem);
    after = pmm_get_used_mem();
    sprintf("\n%lx leaked (%lx to %lx)", after - before, before, after);

    if (is_echfs) {
        sprintf("\nMain directory block: %lu", filesystem.main_dir_block);

        before = pmm_get_used_mem();
        echfs_dir_entry_t *entry0 = echfs_read_dir_entry(&filesystem, 3);
        after = pmm_get_used_mem();
        sprintf("\n%lx leaked (%lx to %lx)", after - before, before, after);

        sprintf("\nFile name: ");
        sprintf(entry0->name);

        uint64_t start = stopwatch_start();
        before = pmm_get_used_mem();
        char *file = echfs_read_file(&filesystem, entry0);
        after = pmm_get_used_mem();
        uint64_t time_elapsed = stopwatch_stop(start);
        sprintf("\n%lx leaked (%lx to %lx)", after - before, before, after);

        sprintf("Size: %lu", strlen(file));
        
        
        //sprintf("\nFile:\n");
        //sprint(file);

        sprintf("\nDone. Took %lu ms.", time_elapsed);

        kfree(entry0);
        kfree(file);
    }
}