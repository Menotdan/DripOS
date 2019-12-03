#pragma once
#include <stdint.h>

#define TABLE_CONSTANT 0xAA12BB34

typedef struct dripfs_boot_sect
{
    uint32_t sector_count;
    char volume_name[20];
    uint32_t root_dir_sector;
    uint32_t first_table_sector;
    char reserved[478];
    uint16_t boot_sig;
} __attribute__((__packed__)) dripfs_boot_sect_t;

typedef struct dripfs_dir_entry
{
    uint8_t id; // set to 1 for folder
    char folder_name[50];
    uint32_t file_table_sector;
    uint32_t parent_dir_entry_sector;
    char reserved[449];
} __attribute__((__packed__)) dripfs_dir_entry_t;

typedef struct dripfs_sector_table
{
    uint32_t sectors[128];
} __attribute__((__packed__)) dripfs_sector_table_t;

typedef struct dripfs_file_entry
{
    uint8_t id; // set to 0 for file
    char filename[50];
    uint32_t sector_of_sector_table;
    uint32_t parent_dir_entry_sector;
    char reserved[449];
} __attribute__((__packed__)) dripfs_file_entry_t;

typedef struct dripfs_128_128
{
    uint32_t index1;
    uint32_t index2;
    uint8_t err;
} dripfs_128_128_t;

typedef struct dripfs_128_128_128
{
    uint32_t index1;
    uint32_t index2;
    uint32_t index3;
    uint8_t err;
} dripfs_128_128_128_t;

void dfs_format(char *volume_name, uint8_t dev, uint8_t controller);