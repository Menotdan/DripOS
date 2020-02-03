#pragma once
#include <stdint.h>
uint16_t ata_buffer[256];
uint16_t ata_drive;
uint32_t ata_controler;
int ata_pio;

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
    MASTER_DRIVE_PIO48 = 0x40,
    SLAVE_DRIVE_PIO48 = 0x50,
    PRIMARY_IDE = 0x1F0,
    SECONDARY_IDE = 0x170
};

enum RW { READ = 0x20, WRITE = 0x30 };

enum PIO { p28 = 0, p48 = 1 };

typedef struct hdd_size {
    uint32_t MAX_LBA;
    uint16_t MAX_LBA_HIGH;
    uint8_t HIGH_USED;
} __attribute__((packed)) hdd_size_t;

void clear_ata_buffer();
int ata_pio28(uint16_t base, uint8_t type, uint16_t drive, uint32_t addr);
int ata_pio48(uint16_t base, uint8_t type, uint16_t drive, uint32_t addr);
void init_hdd();
void drive_scan();
hdd_size_t drive_sectors(uint8_t devP, uint8_t controllerP);

int mp;
int sp;
int ms;
int ss;
int mp48;
int sp48;
int ss48;
int ms48;
int nodrives;