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

uint32_t cluster_to_sector(uint32_t clusterP) {
    return (fat_sectors_used + OTHER_SECTORS + (clusterP*sectors_per_cluster));
}

void write_table(uint32_t *tablep) {
    wait(10);
    uint32_t *tmp = tablep;
    memory_copy((uint8_t *)tmp, writeBuffer, 512);
    writeFromBuffer((OTHER_SECTORS+fat_sector_loaded));
}

uint32_t get_cluster_value(uint32_t cluster) {
    uint32_t temp = cluster;
    uint32_t to_load = 0;
    while (temp >= 128) {
        temp -= 128;
        to_load += 1;
    }
    fat_sector_loaded = to_load;
    readToBuffer((OTHER_SECTORS+fat_sector_loaded)); // Read the FAT, starting at the FAT's location
    memory_copy(readBuffer, (uint8_t *)new_table, 512); // Write the read data to the table
    wait(2);
    temp -= 1;
    return new_table[temp];
}

void setup_entry_dir(dir_entry_t *entry, char name[], uint32_t cluster, uint32_t sector_to_write, uint32_t entry_to_write) {
    for (uint32_t i = 0; i < (uint32_t)strlen(name); i++)
    {
        char toset;
        if (name[i] == 0) {
            toset = remove_null(" ");
        } else {
            char temp[2];
            temp[0] = name[i];
            temp[1] = 0;
            toset = remove_null(temp);
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
    entry->attrib = (entry->attrib | 0x10);
    dir_entry_t *buffer_offset = (dir_entry_t *)writeBuffer;
    buffer_offset += (entry_to_write*sizeof(dir_entry_t));
    memory_copy((uint8_t *)entry, (uint8_t *)buffer_offset, sizeof(dir_entry_t));
    writeFromBuffer(sector_to_write);
}

void setup_entry_file(dir_entry_t *entry, char name[], char ext[], uint32_t cluster, uint32_t size, uint32_t sector_to_write, uint32_t entry_to_write) {
    for (uint32_t i = 0; i < (uint32_t)strlen(name); i++)
    {
        char toset; // Temporary storage for the next char in the name
        if (name[i] == 0) {
            toset = remove_null(" "); // If we found a null byte
        } else {
            char temp[2];
            temp[0] = name[i];
            temp[1] = 0;
            toset = remove_null(temp);
        }
        entry->name[i] = toset; // Set the data
    }
    if (strlen(name) < 8) { // If the string is shorter than the supported size, pad it with spaces
        uint8_t dif = 8-strlen(name);
        for (uint32_t i = 8-dif; i < 8; i++)
        {
            entry->name[i+0] = remove_null(" ");
        } 
    }

    for (uint32_t i = 0; i < (uint32_t)strlen(ext); i++)
    {
        char toset;
        if (ext[i] == 0) {
            toset = remove_null(" ");
        } else {
            toset = ext[i];
        }
        entry->ext[i] = toset;
    }

    if (strlen(name) < 3) {
        uint8_t dif = 3-strlen(name);
        for (uint32_t i = 3-dif; i < 3; i++)
        {
            entry->ext[i] = remove_null(" ");
        } 
    }

    entry->clusterlow = (uint16_t)(cluster & 0xFFFF);
    entry->clusterhigh = (uint16_t)(cluster >> 16);
    entry->attrib = 0x0;
    entry->filesize = size;
    dir_entry_t *buffer_offset = (dir_entry_t *)writeBuffer;
    buffer_offset += (entry_to_write*sizeof(dir_entry_t));
    memory_copy((uint8_t *)entry, (uint8_t *)buffer_offset, sizeof(dir_entry_t));
    writeFromBuffer(sector_to_write);
}

list_t *get_chain(uint32_t cluster_start) {
    uint32_t value; // Setup value
    value = get_cluster_value(cluster_start);
    list_t *cluster_list = new_list(value);
    if (value == 0 || value >= 0x0FFFFFF8 || value == 0x0FFFFFF7) {
        sprintd("Nice chain");
    }
    while (value != 0 && value < 0x0FFFFFF7) {
        //sprintd("Test code");
        value = get_cluster_value(value);
        add_at_end(value, cluster_list);
    }
    return cluster_list;
}

void format() {

    fat32_bpb_t *BPB = kmalloc(sizeof(fat32_bpb_t));
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
    

    memory_copy((uint8_t *)BPB, writeBuffer, 512);
    writeFromBuffer(0);
    fat32_fsinfo_t *fsinfo = kmalloc(sizeof(fat32_fsinfo_t));
    fsinfo->signature = 0x41615252;
    fsinfo->signature2 = 0x61417272;
    fsinfo->free_cluster_count = 0xFFFFFFFF;
    fsinfo->cluster_check_location = 0xFFFFFFFF;
    for (uint32_t i = 0; i<480; i++) {
        fsinfo->reserved[i] = 0;
    }
    memory_copy((uint8_t *)fsinfo, writeBuffer, 512);
    writeFromBuffer(1);
    free(fsinfo, sizeof(fat32_fsinfo_t));
    free(BPB, sizeof(fat32_bpb_t));
    wait(2);
    fat32_bpb_t *tmp2 = kmalloc(sizeof(fat32_bpb_t));
    readToBuffer(0);
    memory_copy(readBuffer, (uint8_t *)tmp2, 512);
    free(tmp2, sizeof(fat32_bpb_t));

    // FSInfo and BPB written, now for creation of the root directory
    
    new_table = kmalloc(512); // Allocates 512 bytes so we can read 128 clusters at a time
    // Allocate two clusters for root directory
    new_table[1] = 3;
    new_table[2] = 0x0FFFFFF8; // End of chain

    uint32_t start_sector = cluster_to_sector(2);
    dir_entry_t *directories = kmalloc(sizeof(dir_entry_t) * ENTRIES_PER_SECTOR);
    setup_entry_dir(directories, "/", 2, start_sector, 0);
    // Write table
    write_table(new_table);
    // Drive has been formatted
    free(new_table, 512);
}

void init_fat() {
    // Read the BIOS parameter block for drive info
    drive_data = kmalloc(sizeof(fat32_bpb_t));
    readToBuffer(0);
    memory_copy(readBuffer, (uint8_t *)drive_data, 512);
    get_drive_clusters(drive_data->sectors_per_cluster);
    sprint_uint(clusters_on_drive);
    sectors_per_cluster = drive_data->sectors_per_cluster;

    // Load FAT
    new_table = kmalloc(512);
}



/* Use an entry to get a files contents */
void read_data_from_entry(dir_entry_t *file, uint32_t *data_out) {
    uint32_t cluster = (uint32_t)file->clusterhigh << 16 | (uint32_t)file->clusterlow;
    uint32_t sectors_to_read = file->filesize / 512;
    uint32_t cur_sector = cluster_to_sector(cluster);
    uint32_t *tmp_out = data_out;

    if (file->filesize % 512 != 0) {
        sectors_to_read += 1;
    }
    sprint("\nCluster: ");
    sprint_uint(file->clusterlow);
    sprint("\nSize: ");
    sprint_uint(file->filesize);

    for (uint32_t i = 0; i < sectors_to_read; i++)
    {
        readToBuffer(cur_sector);
        memory_copy(readBuffer, (uint8_t *)tmp_out, 512);
        tmp_out += 512;
        cur_sector++;
    }
}

/* Use an entry to write to a file */
void write_data_to_entry(dir_entry_t *file, uint32_t *data_in, uint32_t toWrite) {
    uint32_t cluster = (uint32_t)file->clusterhigh << 16 | (uint32_t)file->clusterlow;
    uint32_t sectors_to_read = toWrite / 512;
    uint32_t cur_sector = cluster_to_sector(cluster);
    uint32_t *tmp_out = data_in;

    if (toWrite % 512 != 0) {
        sectors_to_read += 1;
    }
    
    for (uint32_t i = 0; i < sectors_to_read; i++) {
        memory_copy((uint8_t *)tmp_out, writeBuffer, 512);
        writeFromBuffer(cur_sector);
        tmp_out += 512;
        cur_sector++;
    }
}

/* Create a new file */
void new_file(char *name, char *ext, uint32_t pointer, uint32_t size) {
    uint32_t *address = (uint32_t *)get_pointer(pointer);
    dir_entry_t *entry_to_write;
    uint32_t cur_sector = cluster_to_sector(2); // Get 
    uint32_t cur_entry = 0;
    for (uint32_t i = 0; i < ENTRIES_PER_DIR; i++) {
        free(address, sizeof(dir_entry_t));
        entry_to_write = get_entry(cur_sector, cur_entry);
        *address = (uint32_t)entry_to_write;

        if (entry_to_write->filesize == 0 && (entry_to_write->attrib & 0x10) == 0 && (entry_to_write->clusterlow == 0 && entry_to_write->clusterhigh == 0)) {
            sprint("\nUsing entry: ");
            sprint_uint(cur_entry);
            sprint("\nOn sector: ");
            sprint_uint(cur_sector);
            break;
        }

        if (cur_entry == (ENTRIES_PER_SECTOR-1)) {
            cur_entry = 0;
            cur_sector++;
        } else {
            cur_entry++;
        }
    }
    setup_entry_file(entry_to_write, name, ext, 3, size, cur_sector, cur_entry);
    *address = (uint32_t)entry_to_write;
    sprint("\nCluster: ");
    sprint_uint(entry_to_write->clusterlow);
    sprint("\nSize: ");
    sprint_uint(entry_to_write->filesize);
}

/* Get the entryth entry in the clusterth cluster on the currently selected drive */
dir_entry_t *get_entry(uint32_t sector, uint32_t entry) {
    dir_entry_t *sector_data = kmalloc(512);
    dir_entry_t *ret = kmalloc(sizeof(dir_entry_t));
    uint32_t sector_to_read = sector;
    readToBuffer(sector_to_read);
    memory_copy(readBuffer, (uint8_t *)sector_data, 512);
    sector_data += (entry * sizeof(dir_entry_t));
    memory_copy((uint8_t *)sector_data, (uint8_t *)ret, sizeof(dir_entry_t));
    free(sector_data, 512);
    return ret;
}