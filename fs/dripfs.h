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
} dripfs_boot_sect_t;

typedef struct dripfs_dir_entry
{
    char folder_name[50];
    uint32_t file_table_sector;
    uint32_t parent_dir_entry_sector;
    char reserved[450];
} dripfs_dir_entry_t;

typedef struct dripfs_sector_table
{
    uint32_t sectors[128];
} dripfs_sector_table_t;

typedef struct dripfs_file_entry
{
    char filename[50];
    uint32_t sector_of_sector_table;
    uint32_t parent_dir_entry_sector;
    char reserved[450];
} dripfs_file_entry_t;


void dfs_format(char *volume_name, uint8_t dev, uint8_t controller);