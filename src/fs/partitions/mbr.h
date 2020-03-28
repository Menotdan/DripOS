#ifndef PART_MBR_H
#define PART_MBR_H
#include <stdint.h>

typedef struct {
    uint8_t attr;
    uint8_t chs_start_part[3];
    uint8_t part_type;
    uint8_t chs_last_part[3];

    uint32_t lba_part_start;
    uint32_t lba_sector_cnt;
} __attribute__((packed)) mbr_partition_entry_t;

typedef struct {
    uint8_t bootloader[446]; // Ignore the disk ID etc.
    mbr_partition_entry_t partitions[4];
    uint8_t bootloader_magic[2]; // 55aa
} __attribute__((packed)) mbr_bootsect_t;

typedef struct {
    uint64_t offset;
    uint64_t size;
    uint64_t sector_size;
    char *drive;
} mbr_part_offset_t;

void read_mbr(char *drive_name, uint64_t sector_size);

#endif