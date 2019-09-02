#pragma once
#include "../libc/stdint.h"
uint16_t ata_buffer[256];
uint16_t ata_drive;
uint32_t ata_controler;
enum DRIVE_PORTS {
    DATA = 0x0,
    SECTOR_SET1 = 0x1,
    SECTOR_SET2 = 0x2,
    SECTOR_SET3 = 0x3,
    SECTOR_SET4 = 0x4,
    SECTOR_SET5 = 0x5,
    DRIVE_SELECT = 0x6,
    READ_OR_WRITE = 0x7
};

enum DRIVES {
    MASTER_DRIVE = 0xE0,
    SLAVE_DRIVE = 0xF0,
    PRIMARY_IDE = 0x1F0,
    SECONDARY_IDE = 0x170
};

enum RW {
    READ = 0x20,
    WRITE = 0x30
};

enum PIO {
    p28 = 0,
    p48 = 1
};


void clear_ata_buffer();
int ata_pio28(uint16_t base, uint8_t type, uint16_t drive, uint32_t addr);