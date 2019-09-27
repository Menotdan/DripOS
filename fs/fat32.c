#include <stdint.h>
#include <libc.h>
#include <time.h>
#include <serial.h>
#include <stdio.h>
#include "hddw.h"
#include "hdd.h"
#include "fat32.h"

uint32_t *new_table;
fat32_bpb_t *drive_data;
uint32_t sectors_per_cluster;
hdd_size_t driveSize;
uint32_t clusters_on_drive;
uint32_t fat_sectors_used;
uint32_t fat_sector_loaded;

uint32_t cluster_to_sector(uint32_t clusterP) {
    sprint("\nclusterP: ");

    return (fat_sectors_used + OTHER_SECTORS + (clusterP*sectors_per_cluster));
}

uint32_t get_cluster_value(uint32_t cluster) {
    uint32_t temp = cluster;
    uint32_t to_load = 0;
    uint32_t *tmp = new_table;
    //sprintd("Starting loop...");
    //sprint("\nTemp: ");
    //sprint_uint(temp);
    //sprint("\n");
    while (temp >= 128) {
        temp -= 128;
        to_load += 1;
        //sprint("\nArray pos: ");
        //sprint_uint(temp);
        //sprint("\n");
    }

    //if (fat_sector_loaded != to_load) {
        fat_sector_loaded = to_load;
        //sprint("\nReading from sector: ");
        //sprint_uint((OTHER_SECTORS+fat_sector_loaded));
        //sprint("\n");
        //sprint("\nReading from array pos: ");
        //sprint_uint(temp);
        //sprint("\n");

        readToBuffer((OTHER_SECTORS+fat_sector_loaded)); // Read the FAT, starting at the FAT's location
        memory_copy(readBuffer, new_table, 512); // Write the read data to the table
        wait(2);
        // while (new_table[1] == 0) {
        //     readToBuffer((OTHER_SECTORS+fat_sector_loaded)); // Read the FAT, starting at the FAT's location
        //     memory_copy(readBuffer, new_table, 512); // Write the read data to the table
        //     wait(2);
        // }
    //}
    temp -= 1;
    //for (uint32_t i = 0; i < 128; i++) {
    //    sprint("\nData: ");
    //    sprint_uint(new_table[i]);
    //    sprint(" I: ");
    //    sprint_uint(i);
    //}
    //sprint("\nSector: ");
    //sprint_uint((OTHER_SECTORS+fat_sector_loaded));
    return new_table[temp];
}

list_t *get_chain(uint32_t cluster_start) {
    sprintd("Getting chain");
    uint32_t value; // Setup value
    value = get_cluster_value(cluster_start);
    list_t *cluster_list = new_list(value);
    sprint("\nTest: ");
    sprint_uint(value);
    sprint("\n");
    if (value == 0 || value >= 0x0FFFFFF8 || value == 0x0FFFFFF7) {
        sprintd("REEE");
    }
    while (value != 0 && value < 0x0FFFFFF7) {
        //sprintd("Test code");
        value = get_cluster_value(value);
        sprint("\nTest Loop: ");
        sprint_uint(value);
        sprint("\n");
        add_at_end(value, cluster_list);
    }
    //remove_at_end(cluster_list);
    sprint("\nTest: ");
    sprint_uint(value);
    sprint("\n");
    return cluster_list;
}

void format() {
    sprintd("Formatting drive...");

    fat32_bpb_t *BPB = kmalloc(sizeof(fat32_bpb_t));
    sprint("Size of table: ");
    sprint_uint(sizeof(fat32_bpb_t));
    sprint(" bytes\n");
    BPB->skip_vol_info_high = 0xEBFE;
    BPB->skip_vol_info_low = 0x90;
    BPB->oem_name[0] = remove_null("D");
    BPB->oem_name[1] = remove_null("R");
    BPB->oem_name[2] = remove_null("P");
    BPB->oem_name[3] = remove_null("O");
    BPB->oem_name[4] = remove_null("S");
    BPB->oem_name[5] = remove_null("0");
    BPB->oem_name[6] = remove_null(".");
    BPB->oem_name[7] = remove_null("0");

    BPB->volume_label[0] = remove_null("D");
    BPB->volume_label[1] = remove_null("R");
    BPB->volume_label[2] = remove_null("I");
    BPB->volume_label[3] = remove_null("P");
    BPB->volume_label[4] = remove_null(" ");
    BPB->volume_label[5] = remove_null(" ");
    BPB->volume_label[6] = remove_null(" ");
    BPB->volume_label[7] = remove_null(" ");
    BPB->volume_label[8] = remove_null(" ");
    BPB->volume_label[9] = remove_null(" ");
    BPB->volume_label[10] = remove_null(" ");

    BPB->ID_string[0] = remove_null("F");
    BPB->ID_string[1] = remove_null("A");
    BPB->ID_string[2] = remove_null("T");
    BPB->ID_string[3] = remove_null("3");
    BPB->ID_string[4] = remove_null("2");
    BPB->ID_string[5] = remove_null(" ");
    BPB->ID_string[6] = remove_null(" ");
    BPB->ID_string[7] = remove_null(" ");

    BPB->bytes_per_sector = 512;
    BPB->sectors_per_cluster = 3;
    get_drive_clusters(BPB->sectors_per_cluster);
    sectors_per_cluster = BPB->sectors_per_cluster;
    BPB->res_sectors = fat_sectors_used + OTHER_SECTORS;
    BPB->number_of_fats = 1;
    BPB->directory_entries = 1;
    BPB->small_sector_count = 0;
    BPB->drive_type = 0xD1;
    BPB->sectors_per_fat = 0;
    BPB->sectors_per_track = 0;
    BPB->heads = 0;
    BPB->hidden_sectors = 0;
    BPB->large_sector_count = driveSize.MAX_LBA;
    BPB->sectors_per_fat32 = fat_sectors_used;
    BPB->flags = 0;
    BPB->version = 0x0202;
    BPB->root_dir_cluster = 2;
    BPB->fsinfo_sector = 1;
    BPB->backup_boot = 0;
    BPB->drive_number = 0x80;
    BPB->signature = 0x28;
    BPB->volume_serial = 0xD719D719;
    BPB->boot_sig = 0xAA55;

    for (uint32_t i = 0; i < 420; i++)
    {
        BPB->boot_code[i] = 0;
    }
    

    memory_copy(BPB, writeBuffer, 512);
    writeFromBuffer(0);
    fat32_fsinfo_t *fsinfo = kmalloc(sizeof(fat32_fsinfo_t));
    fsinfo->signature = 0x41615252;
    fsinfo->signature2 = 0x61417272;
    fsinfo->free_cluster_count = 0xFFFFFFFF;
    fsinfo->cluster_check_location = 0xFFFFFFFF;
    for (uint32_t i; i<480; i++) {
        fsinfo->reserved[i] = 0;
    }
    memory_copy(fsinfo, writeBuffer, 512);
    writeFromBuffer(1);
    free(fsinfo, sizeof(fat32_fsinfo_t));
    free(BPB, sizeof(fat32_bpb_t));
    wait(2);
    fat32_bpb_t *tmp2 = kmalloc(sizeof(fat32_bpb_t));
    readToBuffer(0);
    memory_copy(readBuffer, tmp2, 512);
    sprint("\nBytes per sector: ");
    sprint_uint(tmp2->bytes_per_sector);
    sprint("\nLarge sector count: ");
    sprint_uint(tmp2->large_sector_count);
    sprint("\nVolume serial: ");
    sprint_uint(tmp2->volume_serial);
    sprint("\nBoot signature: ");
    sprint_uint(tmp2->boot_sig);
    sprint("\n");
    free(tmp2, sizeof(fat32_bpb_t));

    // FSInfo and BPB written, now for creation of the root directory
    
    new_table = kmalloc(512); // Allocates 512 bytes so we can read 128 clusters at a time
    uint32_t ok = get_cluster_value(2);
    // Allocate two clusters for root directory
    new_table[1] = 3;
    new_table[2] = 0x0FFFFFF8; // End of chain

    uint32_t start_sector = cluster_to_sector(2);
    uint32_t last_sector = cluster_to_sector(3) + sectors_per_cluster;
    // Write table
    write_table(new_table);
    // Drive has been formatted
    sprintd("The drive has been formatted");
    free(new_table, 512);
}

void init_fat() {
    sprintd("Loading drive...");

    // Read the BIOS parameter block for drive info
    drive_data = kmalloc(sizeof(fat32_bpb_t));
    readToBuffer(0);
    memory_copy(readBuffer, drive_data, 512);
    while (drive_data->large_sector_count == 0) {
        readToBuffer(0);
        memory_copy(readBuffer, drive_data, 512);
        //sprintd("Still loading drive...");
    }
    get_drive_clusters(drive_data->sectors_per_cluster);
    sectors_per_cluster = drive_data->sectors_per_cluster;

    // Load FAT
    new_table = kmalloc(512);

    uint32_t ok = get_cluster_value(2);

    list_t *root_dir = get_chain(2);

    sprintd("Drive loaded");
}

void setup_entry_dir(dir_entry_t *entry, char name[], uint32_t cluster) {
    for (uint32_t i = 0; i < strlen(name); i++)
    {
        char toset;
        if (name[i] == 0) {
            toset = remove_null(" ");
        } else {
            toset = name[i];
        }
        entry->name[i] = toset;
    }

    if (strlen(name) < 8) {
        uint8_t dif = 8-strlen(name);
        for (uint32_t i = 8-dif; i < 8; i++)
        {
            entry->name[i] = remove_null(" ");
        } 
    }
    entry->clusterlow = (uint16_t)(cluster & 0xFFFF);
    entry->clusterhigh = (uint16_t)(cluster >> 16);
    entry->attrib = 0x10;
}

void write_table(uint32_t *tablep) {
    wait(10);
    uint32_t *tmp = tablep;
    memory_copy(tmp, writeBuffer, 512); // Copy the table to memory
    writeFromBuffer((OTHER_SECTORS+fat_sector_loaded)); // Read the FAT, starting at the FAT's location
}

void get_drive_size() {
    if (ata_controler == PRIMARY_IDE)
    {
        if (ata_drive == MASTER_DRIVE)
        {
            driveSize = drive_sectors(1,1);
        } else
        {
            driveSize = drive_sectors(0,1);
        }
        
    } else
    {
        if (ata_drive == MASTER_DRIVE)
        {
            driveSize = drive_sectors(1,0);
        } else
        {
            driveSize = drive_sectors(0,0);
        }
    }
}

void get_drive_clusters(uint32_t spc) {
    get_drive_size();
    clusters_on_drive = (driveSize.MAX_LBA / spc);
    if (driveSize.MAX_LBA % spc != 0) {
        clusters_on_drive += 1;
    }
    uint32_t temp_fat_sectors = (clusters_on_drive*sizeof(uint32_t))/512;
    if ((clusters_on_drive*sizeof(uint32_t)) % 512 != 0)
    {
        temp_fat_sectors += 1;
    }
    fat_sectors_used = temp_fat_sectors;
}

