// Drip FS

// A garbage FS written by Menotdan

#include "hddw.h"
#include "hdd.h"
#include "dripfs.h"
#include "../libc/mem.h"
#include "../include/debug.h"
#include "../libc/string.h"

void dfs_format(char *volume_name, uint8_t dev, uint8_t controller) {
    dripfs_boot_sect_t *boot_sect = kmalloc(512); // Allocate one sector in memory for the boot sector
    hdd_size_t driveSize = drive_sectors(dev, controller); // Get the drive size
    boot_sect->sector_count = driveSize.MAX_LBA; // Set sector_count property
    asserta(strlen(volume_name) < 20, "Volume name too long!"); // Make sure volume name is short enough
    strcpy(boot_sect->volume_name, volume_name); // Copy volume name to the boot sector
    boot_sect->root_dir_sector = 1; // Root directory sector
    boot_sect->first_table_sector = 2; // First table of tables (the core space waster of Drip FS lol)
    boot_sect->boot_sig = 0x55aa;

    /* Write the table signature to all sectors */
    uint32_t *drive_buffer = (uint32_t *) writeBuffer;
    for (uint32_t x = 0; x<128; x++) {
        *(drive_buffer + x) = 0;
    }
    for (uint32_t i = 0; i<driveSize.MAX_LBA; i++) {
        *drive_buffer = TABLE_CONSTANT;
        writeFromBuffer(i);
    }
    memory_copy((uint8_t *)boot_sect, writeBuffer, 512);
    writeFromBuffer(0);
}