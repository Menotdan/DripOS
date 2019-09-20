#ifndef FAT32_H
#define FAT32_H

#include <stdint.h>

#define FAT_SECTORS 200
#define OTHER_SECTORS 3

typedef struct fat32_bpb // BIOS parameter block to be written to the drive
{
    uint16_t skip_vol_info_high; uint8_t skip_vol_info_low; // Code to skip volume info
    char oem_name[8]; // Name of the OS that formatted this disk
    uint16_t bytes_per_sector; // Number of bytes per sector, probably 512
    uint8_t sectors_per_cluster; // Number of sectors per cluster
    uint16_t res_sectors; // Reserved sectors
    uint8_t number_of_fats; // The number of FATs on the drive
    uint16_t directory_entries; // The number of directory entries
    uint16_t small_sector_count; // The small sector count. 0 on drives larger than 65535 sectors
    uint8_t drive_type; // Planning on making a custom type for DripOS since it is likely ignored by modern OSes
    uint16_t sectors_per_fat; // Only set on FAT 12/16
    uint16_t sectors_per_track; // Sectors per track
    uint16_t heads; // Number of heads on the drive
    uint32_t hidden_sectors; // Hidden sectors
    uint32_t large_sector_count;
    // Extended BPB starts here
    uint32_t sectors_per_fat32; // Sectors per FAT on FAT32
    uint16_t flags; // Flags
    uint16_t version; // FAT version
    uint32_t root_dir_cluster; // The cluster of the root directory, often 2
    uint16_t fsinfo_sector; // The sector of the FSInfo structure
    uint16_t backup_boot; // The backup boot sector
    uint32_t reserved1, reserved2, reserved3; // Reserved bytes that are set to 0 when the drive is formatted
    uint8_t drive_number; // 0x80 for hard disks, 0x0 for floppy disks
    uint8_t windows_nt_flags; // Reserved for Windows NT
    uint8_t signature; // IDK im just gonna set it to 0x29
    uint32_t volume_serial; // I am gonna use 0xD719 for DripOS
    char volume_label[11]; // Volume label
    char ID_string[8]; // "FAT32   "
    uint8_t boot_code[420];
    uint16_t boot_sig; // Boot signature (0xAA55)
} __attribute__((packed)) fat32_bpb_t;

typedef struct fat32_fsinfo // FSInfo structure used by FAT32
{
    uint32_t signature; // Signature DWORD, must be 0x41615252
    uint8_t reserved[480]; // Unused reserved bytes
    uint32_t signature2; // Second signature, must be 0x61417272
    uint32_t free_cluster_count; // Last known free cluster count. 0xFFFFFFFF means the count is unknown
    uint32_t cluster_check_location; // Last known cluster location 0xFFFFFFFF means the value is unknown
} __attribute__((packed)) fat32_fsinfo_t;

typedef struct dir_entry
{
	char name[8];
	char ext[3];
	uint8_t attrib;
	uint8_t userattrib;
 
	char undelete;
	uint16_t createtime;
	uint16_t createdate;
	uint16_t accessdate;
	uint16_t clusterhigh;
 
	uint16_t modifiedtime;
	uint16_t modifieddate;
	uint16_t clusterlow;
	uint32_t filesize;
} __attribute__ ((packed)) dir_entry_t;

typedef struct fat
{
    dir_entry_t entries[3657];
} __attribute__ ((packed)) fat_t;

void format();
#endif