#include <stdint.h>
#include <libc.h>
#include <time.h>
#include <serial.h>
#include <stdio.h>
#include "hddw.h"
#include "fat32.h"


void format() {
    fat32_bpb_t *BPB = kmalloc(sizeof(fat32_bpb_t));
    sprint("Size of table: ");
    sprint_uint(sizeof(fat32_bpb_t));
    sprint(" bytes\n");
    BPB->skip_vol_info_high = 0xEBFE;
    BPB->skip_vol_info_low = 0x90;
    BPB->oem_name[0] = "D";
    BPB->oem_name[1] = "R";
    BPB->oem_name[2] = "P";
    BPB->oem_name[3] = "O";
    BPB->oem_name[4] = "S";
    BPB->oem_name[5] = "0";
    BPB->oem_name[6] = ".";
    BPB->oem_name[7] = "0";
    BPB->volume_label[10] = " ";
    BPB->ID_string[7] = " ";

    BPB->bytes_per_sector = 512;
    BPB->sectors_per_cluster = 3;
    BPB->res_sectors = FAT_SECTORS + OTHER_SECTORS;
    BPB->number_of_fats = 1;
    BPB->directory_entries = 1;
    BPB->small_sector_count = 0;
    BPB->drive_type = 0xD1;
    BPB->sectors_per_fat = 0;
    BPB->sectors_per_track = 0;
    BPB->heads = 0;
    BPB->hidden_sectors = 0;
    BPB->large_sector_count = 1234;
    BPB->sectors_per_fat32 = FAT_SECTORS;
    BPB->flags = 0;
    BPB->version = 0x0101;
    BPB->root_dir_cluster = 2;
    BPB->fsinfo_sector = 1;
    BPB->backup_boot = 2;
    BPB->drive_number = 0x80;
    BPB->signature = 0x29;
    BPB->volume_serial = 0xD719;
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
    fsinfo->cluster_check_location = 2;
    for (uint32_t i; i<480; i++) {
        fsinfo->reserved[i] = 0;
    }
    memory_copy(fsinfo, writeBuffer, 512);
    writeFromBuffer(1);
    free(fsinfo, sizeof(fat32_fsinfo_t));
    free(BPB, sizeof(fat32_bpb_t));
    wait(2);
    fat32_bpb_t *tmp = kmalloc(sizeof(fat32_bpb_t));
    readToBuffer(0);
    memory_copy(readBuffer, tmp, 512);
    sprint("\nBytes per sector: ");
    sprint_uint(tmp->bytes_per_sector);
    sprint("\nLarge sector count: ");
    sprint_uint(tmp->large_sector_count);
    sprint("\nVolume serial: ");
    sprint_uint(tmp->volume_serial);
    sprint("\nBoot signature: ");
    sprint_uint(tmp->boot_sig);
    sprint("\n");
    free(tmp, sizeof(fat32_bpb_t));
    // FSInfo and BPB written, now for creation of the root directory
    fat_t *FAT = kmalloc(sizeof(fat_t));
    fat_t *tmp = FAT;
    for (uint32_t i = 0; i < FAT_SECTORS; i++) {
        read(3+i, 0);
        memory_copy(readBuffer, tmp, 512);
    }
}