// Drip FS
// "Where slowness is not a bad thing"
// A garbage FS written by Menotdan

#include "hddw.h"
#include "hdd.h"
#include "dripfs.h"
#include "../libc/mem.h"
#include "../include/debug.h"
#include "../libc/string.h"
#include "../drivers/screen.h"
#include "../drivers/serial.h"
#include "../cpu/timer.h"

dripfs_boot_sect_t *boot_sect;

uint32_t dfs_table_scan(uint32_t first_sector, uint8_t dev, uint8_t controller) {
    /* Code to set drive */
    //uint8_t driveSelected = 1;
    if (dev == 0 && controller == 0) {
        //driveSelected = current_drive;
    } else {
        if (dev == ata_drive && controller == ata_controler) {
            //driveSelected = current_drive;
        }
    }
    /* Table scanner */
    for (uint32_t s = first_sector; s<boot_sect->sector_count; s++) {
        readToBuffer(s);
        if (*(uint32_t*)readBuffer == TABLE_CONSTANT) {
            sprint("\nTable scanner got: ");
            sprint_uint(s);
            *(uint32_t*)readBuffer = 0;
            memory_copy(readBuffer, writeBuffer, 512);
            writeFromBuffer(s, 0);
            return s;
        }
    }
    sprint("\nwelppppp");
    return -1;
}

dripfs_128_128_t dfs_table_find_128x128(dripfs_sector_table_t *start_table, uint32_t table_sector) {
    /* Scanning section */
    uint32_t current_index = 0; // Store current index
    uint32_t current_index_2 = 0; // Index for the second 128 table
    uint8_t good = 0;
    uint8_t empty = 0;
    dripfs_sector_table_t *scan_table = kmalloc(512); // Create a table space
    dripfs_128_128_t ret;
    for (current_index = 0; current_index < 128; current_index++) { // Loop to find an empty slot, if it finds nothing, it leaves the current_index variable set to 128
        if (start_table->sectors[current_index] == 0) {
            empty = 1;
            break;
        } 
        readToBuffer(start_table->sectors[current_index]);
        memory_copy(readBuffer, (uint8_t *)scan_table, 512);
        good = 0;
        for (current_index_2 = 0; current_index_2 < 128; current_index_2++) { // Loop to find an empty slot, if it finds nothing, it leaves the current_index variable set to 128
            if (scan_table->sectors[current_index_2] == 0) {
                good = 1;
                break;
            }
        }
        if (current_index_2 != 128 || good == 1) {
            break; // If the for loop didnt make it to the end, then that means there is a blank entry in the table
        }
    }
    if (empty == 1) {
        uint32_t new_1 = dfs_table_scan(1, 0, 0);
        scan_table->sectors[current_index] = new_1;
        memory_copy((uint8_t *)scan_table, readBuffer, 512);
        writeFromBuffer(table_sector, 0);
    }
    current_index_2 = 0;
    free(scan_table, 512);
    if (current_index == 128) {
        current_index = 127; // If the value is max, set it to the actual index of the array
    }
    if (current_index_2 == 128 && current_index == 127 && good != 1) {
        ret.err = 1; // If the second index and the first index are both maxed out, then there was no space found
        return ret;
    }
    if (current_index_2 == 128) {
        current_index_2 = 127; // If the value is max, set it to the actual index of the array
    }
    ret.index1 = current_index;
    ret.index2 = current_index_2;
    return ret;
}

uint32_t dfs_table_128x128_add_new(uint32_t sector_to_write, dripfs_sector_table_t *start_table, uint32_t table_sector) {
    dripfs_128_128_t to_write = dfs_table_find_128x128(start_table, table_sector); // Find an empty part of the disk
    if (to_write.err == 1) {
        return 1;
    }
    dripfs_sector_table_t *table = kmalloc(512);

    readToBuffer(start_table->sectors[to_write.index1]);
    memory_copy(readBuffer, (uint8_t *)table, 512);
    table->sectors[to_write.index2] = sector_to_write;
    memory_copy((uint8_t *)table, writeBuffer, 512);
    writeFromBuffer(start_table->sectors[to_write.index1], 0);
    free(table, 512);
    return 0;
}

dripfs_128_128_128_t dfs_table_find_128x128x128(dripfs_sector_table_t *start_table, uint32_t table_sector) {
    /* Scanning section */
    dripfs_128_128_128_t ret; // Return value
    dripfs_128_128_t temp; // Temp file return array so that I can store data that 128x128 find returns
    uint32_t index = 0; // Index
    dripfs_sector_table_t *temp_table = kmalloc(512); // Temporary table for storing information while reading and writing tables
    uint8_t empty = 0;
    for (index = 0; index < 128; index++) { // Scan through all the entries in the table
        if (start_table->sectors[index] == 0) {
            empty = 1; // Set empty var
            break; // If the table is empty, break
        }
        temp = dfs_table_find_128x128(start_table, start_table->sectors[index]); // Find data in the next table
        if (temp.err != 1) { // If the function found empty space, return the empty space
            if (index > 127) {
                index = 127;
            }
            ret.index1 = index;
            ret.index2 = temp.index1;
            ret.index3 = temp.index2;
            return ret;
        }
    }
    if (index > 127) {
        index = 127;
    }
    if (empty == 1) {
        uint32_t new_sector = dfs_table_scan(1, 0, 0);
        uint32_t old_sector = 0;
        readToBuffer(table_sector);
        memory_copy(readBuffer, (uint8_t *)temp_table, 512);
        temp_table->sectors[index] = new_sector;
        memory_copy((uint8_t *)temp_table, writeBuffer, 512);
        writeFromBuffer(table_sector, 0);
        /* repeat 1 */
        old_sector = new_sector;
        readToBuffer(new_sector);
        memory_copy(readBuffer, (uint8_t *)temp_table, 512);
        new_sector = dfs_table_scan(1, 0, 0);
        temp_table->sectors[0] = new_sector;
        memory_copy((uint8_t *)temp_table, writeBuffer, 512);
        writeFromBuffer(old_sector, 0);
        /* repeat 2 */
        old_sector = new_sector;
        readToBuffer(new_sector);
        memory_copy(readBuffer, (uint8_t *)temp_table, 512);
        new_sector = dfs_table_scan(1, 0, 0);
        temp_table->sectors[0] = 0;
        memory_copy((uint8_t *)temp_table, writeBuffer, 512);
        writeFromBuffer(old_sector, 0);
        // Return data
        ret.index1 = index;
        ret.index2 = 0;
        ret.index3 = 0;
        return ret;
    } else {
        ret.err = 1;
        return ret;
    }
    return ret;
}

void dfs_new_folder(char *name, uint32_t entry_sector, uint32_t table_sector, uint32_t parent_dir_sector, uint8_t dev, uint8_t controller) {
    //uint8_t driveSelected = 1;
    if (dev == 0 && controller == 0) {
        //driveSelected = current_drive;
    } else {
        if (dev == ata_drive && controller == ata_controler) {
            //driveSelected = current_drive;
        }
    }
    dripfs_dir_entry_t *dir = kmalloc(512);
    dir->parent_dir_entry_sector = parent_dir_sector; // Where the parent directory lives
    dir->file_table_sector = table_sector; // Where the file table is locted for this folder
    dir->id = 1; // 1 for folders
    strcpy((char*)&(dir->folder_name), name);
    for (uint32_t s = 0; s < 449; s++) {
        dir->reserved[s] = '\0';
    }
    memory_copy((uint8_t *)dir, writeBuffer, 512);
    sprint("\nEntry sector: ");
    sprint_uint(entry_sector);
    writeFromBuffer(entry_sector, 0);
    dripfs_sector_table_t *table = kmalloc(512);
    for (uint32_t i = 0; i<128; i++) {
        table->sectors[i] = 0;
    }
    memory_copy((uint8_t *)table, writeBuffer, 512); // Copy table to buffer
    sprint("\nTable sector: ");
    sprint_uint(table_sector);
    writeFromBuffer(table_sector, 0);
    free(dir, 512);
    free(table, 512);
}

void dfs_new_file(char *name, uint32_t entry_sector, uint32_t table_sector, uint32_t parent_dir_sector, uint8_t dev, uint8_t controller) {
    //uint8_t driveSelected = 1;
    if (dev == 0 && controller == 0) {
        //driveSelected = current_drive;
    } else {
        if (dev == ata_drive && controller == ata_controler) {
            //driveSelected = current_drive;
        }
    }
    dripfs_file_entry_t *dir = kmalloc(512); // Allocate memory
    dripfs_dir_entry_t *parent_folder = kmalloc(512);
    readToBuffer(parent_dir_sector); // Read the parent directory's entry sector
    memory_copy(readBuffer, (uint8_t *)parent_folder, 512); // Copy it to the allocated memory
    uint32_t file_table = parent_folder->file_table_sector; // Store the table sector in memory
    free(parent_folder, 512); // Free the allocated memory, as we no longer need it
    /* Now we do some table nonsense */
    dripfs_sector_table_t *parent_dir_table = kmalloc(512); // Allocate memory for the table
    readToBuffer(file_table);
    memory_copy(readBuffer, (uint8_t *)parent_dir_table, 512);
    dfs_table_128x128_add_new(entry_sector, parent_dir_table, file_table);

    dir->parent_dir_entry_sector = parent_dir_sector; // Where the parent directory lives
    dir->sector_of_sector_table = table_sector; // Where the file table is locted for this folder
    dir->id = 0; // 0 for files
    strcpy((char*)&(dir->filename), name); // Copy filename
    for (uint32_t s = 0; s < 449; s++) {
        dir->reserved[s] = '\0'; // Fill reserved
    }
    memory_copy((uint8_t *)dir, writeBuffer, 512); // Copy the dir entry to write buffer
    sprint("\nEntry sector: ");
    sprint_uint(entry_sector);
    writeFromBuffer(entry_sector, 0); // Write
    dripfs_sector_table_t *table = kmalloc(512); // Allocate space for table
    for (uint32_t i = 0; i<128; i++) { // Clear table
        table->sectors[i] = 0;
    }
    memory_copy((uint8_t *)table, writeBuffer, 512); // Copy table to buffer
    sprint("\nTable sector: ");
    sprint_uint(table_sector);
    writeFromBuffer(table_sector, 0); // Write table to table sector
    free(dir, 512); // Free space
    free(table, 512);
}

uint32_t dfs_calculate_sectors(uint32_t p) {
    uint32_t sectors_needed = p/512;
    uint32_t modulo = p%512;
    if (modulo != 0) {
        sectors_needed += 1;
    }
    return sectors_needed;
}

void dfs_write_file(dripfs_file_entry_t *file, uint8_t *data, uint32_t bytes, uint8_t append, uint8_t dev, uint8_t controller) {
    //uint8_t driveSelected = 1;
    if (dev == 0 && controller == 0) {
        //driveSelected = current_drive;
    } else {
        if (dev == ata_drive && controller == ata_controler) {
            //driveSelected = current_drive;
        }
    }

    if (append == 0) {
        dripfs_sector_table_t *temp_table; // Temp table
        temp_table = (dripfs_sector_table_t *)readBuffer; // Set pointer to the read buffer
        readToBuffer(file->sector_of_sector_table); // Read to the buffer
        sprint("\nFile name: ");
        sprint(file->filename);
        /* TODO: add support for files that already have data chunks in them and also add support for appending data */
        if (temp_table->sectors[0] == 0) { // Check if the first sector is 0, if it is, we can fill it with a new cell
            uint32_t test = dfs_table_scan(1, 0, 0);
            temp_table->sectors[0] = test; // Scan for a free table and assign the sector of that table to the first entry in the file's table
            memory_copy(readBuffer, writeBuffer, 512); // Copy the modified data to the write buffer
            sprint("\nTable table sector: ");
            sprint_uint(file->sector_of_sector_table);
            writeFromBuffer(file->sector_of_sector_table, 0); // Write
            readToBuffer(temp_table->sectors[0]); // Read the new chunk into memory

            // Repeat the process because DripFS is bad and uses 128*128*128 tables, which are sure to make it slow
            temp_table->sectors[0] = dfs_table_scan(1, 0, 0); // Scan for a free table and assign the sector of that table to the first entry in the file's table
            memory_copy(readBuffer, writeBuffer, 512); // Copy the modified data to the write buffer
            sprint("\nTable table sector: ");
            sprint_uint(test);
            writeFromBuffer(test, 0); // Write
            readToBuffer(temp_table->sectors[0]); // Read the new chunk into memory
            test = temp_table->sectors[0];
            uint32_t sectors_needed = dfs_calculate_sectors(bytes); // Calculate sectors needed to write this many bytes
            /* TODO: add support for when sectors_needed > 128 */
            for (uint32_t sec = 0; sec<sectors_needed; sec++) {
                temp_table->sectors[sec] = dfs_table_scan(1, 0, 0); // Fill the table with as many sectors as we need
            }
            temp_table = kmalloc(512); // Allocate space for a table independent of readBuffer
            memory_copy(readBuffer, (uint8_t *)temp_table, 512); // Copy the readBuffer into temp_table
            memory_copy((uint8_t *)temp_table, writeBuffer, 512); // Copy table into buffer to be written
            writeFromBuffer(test, 0); // Write table
            uint8_t *data_temp = data; // Setup temp data buffer
            for (uint32_t sec = 0; sec<sectors_needed; sec++) {
                memory_copy(data_temp, writeBuffer, 512); // Copy data
                sprint("\nDrive sector: ");
                sprint_uint(temp_table->sectors[sec]);
                writeFromBuffer(temp_table->sectors[sec], 0); // Write the data to whatever sector is the table
                data_temp += 512;
            }
        }
        free(temp_table, 512);
    }
}

void dfs_format(char *volume_name, uint8_t dev, uint8_t controller) {
    //uint8_t driveSelected = 1;
    if (dev == 0 && controller == 0) {
        //driveSelected = current_drive;
    } else {
        if (dev == ata_drive && controller == ata_controler) {
            //driveSelected = current_drive;
        }
    }
    boot_sect = kmalloc(512); // Allocate one sector in memory for the boot sector
    hdd_size_t driveSize = drive_sectors(dev, controller); // Get the drive size
    boot_sect->sector_count = driveSize.MAX_LBA; // Set sector_count property
    kprint("\nThis should take about ");
    kprint_uint(driveSize.MAX_LBA/1024);
    kprint(" seconds");

    asserta(strlen(volume_name) < 20, "Volume name too long!"); // Make sure volume name is short enough
    strcpy((char*)&(boot_sect->volume_name), volume_name); // Copy volume name to the boot sector
    boot_sect->root_dir_sector = 1; // Root directory sector
    boot_sect->first_table_sector = 2; // First table of tables (the core space waster of Drip FS lol)
    boot_sect->boot_sig = 0x55aa;
    for (uint32_t q = 0; q<478; q++) {
        boot_sect->reserved[q] = '\0';
    }
    uint32_t ticks_start = tick;
    /* Write the table signature to all sectors */
    uint32_t *drive_buffer = (uint32_t *) writeBuffer;
    for (uint32_t x = 0; x<128; x++) {
        *(drive_buffer + x) = 0;
    }
    for (uint32_t i = 1; i<driveSize.MAX_LBA; i++) {
        *drive_buffer = TABLE_CONSTANT;
        writeFromBuffer(i, 0);
    }
    memory_copy((uint8_t *)boot_sect, writeBuffer, 512);
    writeFromBuffer(0, 1);
    // Now to create the root directory
    uint32_t entrySector = dfs_table_scan(1, 0, 0); // Find the first fitting sector
    uint32_t tableSector = entrySector + 1; // We can assume that the next sector will be free because of how dfs_table_scan() works
    dfs_new_folder("/", entrySector, tableSector, 0, 0, 0);
    kprint("\nFinished in ");
    kprint_uint((tick-ticks_start)/100);
    kprint(" seconds");
    uint8_t *data = kmalloc(512);
    memory_set(data, 0, 512);
    *data = 123;
    dfs_new_file("test.txt", 3, 4, 1, 0, 0);
    readToBuffer(3);
    dripfs_file_entry_t *read_entry = kmalloc(512);
    memory_copy(readBuffer, (uint8_t *)read_entry, 512);
    sprint("\nData: ");
    sprint(read_entry->filename);
    sprint(" ");
    sprint_uint(read_entry->id);
    dfs_write_file(read_entry, data, 512, 0, 0, 0);
    free(data, 512);
}