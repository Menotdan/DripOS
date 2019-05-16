#pragma once
#include <stdint.h>
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
    PRIMARY = 0xE0,
    SLAVE = 0xF0,
    PRIMARY_BASE = 0x1F0,
    SLAVE_BASE = 0x170
};

enum RW {
    READ = 0x20,
    WRITE = 0x30
};

enum PIO {
    p28 = 0,
    p48 = 1
};

void HD_READ(uint32_t sector, uint16_t *out_buf);
void ata_detect();