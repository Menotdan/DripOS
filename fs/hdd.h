#pragma once
#include <stdint.h>
enum DRIVE_PORTS {
    SECTOR_SET1 = 0x1F1,
    SECTOR_SET2 = 0x1F2,
    SECTOR_SET3 = 0x1F3,
    SECTOR_SET4 = 0x1F4,
    SECTOR_SET5 = 0x1F5,
    DRIVE_SELECT = 0x1F6,
    READ_OR_WRITE = 0x1F7
};

enum DRIVES {
    PRIMARY = 0xE0,
    SLAVE = 0xF0
};

enum RW {
    READ = 0x20,
    WRITE = 0x30
};

void HD_READ(uint32_t sector, uint32_t DRIVE, uint16_t *out_buf);