/*
    hdd.h
    Copyright Menotdan 2018-2019

    HDD Setup

    MIT License
*/

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <math.h>
#include <drivers/time.h>
#include "../cpu/isr.h"

#define IDE_MASTER 0
#define IDE_SLAVE  1

#define IDE_DEFAULT_PRIMARY 0x1F0
#define IDE_DEFAULT_SECONDARY 0x170

#define ATA_PORT_DATA 0x000
#define ATA_PORT_ERROR 0x001
#define ATA_PORT_FEATURES 0x001
#define ATA_PORT_SCT_COUNT 0x002
#define ATA_PORT_SCT_NUMBER 0x003
#define ATA_PORT_CYL_LOW 0x004
#define ATA_PORT_CYL_HIGH 0x005
#define ATA_PORT_DRV 0x006
#define ATA_PORT_STATUS 0x007
#define ATA_PORT_COMMAND 0x007
#define ATA_PORT_ALT_STATUS 0x206
#define ATA_PORT_PRIMARY_DETECT 0x1F3
#define ATA_PORT_PRIMARY_DRIVE_DETECT 0x1F6
#define ATA_PORT_PRIMARY_STATUS 0x1F7
#define ATA_PORT_SECONDARY_DETECT 0x173
#define ATA_PORT_SECONDARY_DRIVE_DETECT 0x176
#define ATA_PORT_SECONDARY_STATUS 0x177

#define MAGIC_DETECT 0x88 // Detect controllers
#define MASTER_DRIVE_DETECT 0xA0 // Master drive detection
#define SLAVE_DRIVE_DETECT 0xB0 // Slave drive detection

#define ATA_SR_BSY     0x80    // Busy
#define ATA_SR_DRQ     0x08    // Data request ready
#define ATA_SR_ERR     0x01    // Error

#define ATA_READ 1
#define ATA_WRITE 2

#define ATA_PIO28 0
#define ATA_PIO48 1

#define CMD_GET_SECTORS 0xF8

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

enum RW {
    READ = 0x20,
    WRITE = 0x30
};

enum PIO {
    p28 = 0,
    p48 = 1
};

typedef struct hdd_size {
    uint32_t MAX_LBA;
    uint16_t MAX_LBA_HIGH;
    uint8_t HIGH_USED;
} __attribute__ ((packed)) hdd_size_t;

uint16_t ata_buffer[256];
uint16_t ata_drive;
uint32_t ata_controler;
int ata_pio;
int mp;
int sp;
int ms;
int ss;
int mp48;
int sp48;
int ss48;
int ms48;
int nodrives;

void clear_ata_buffer();
int ata_pio28(uint16_t base, uint8_t type, uint16_t drive, uint32_t addr);
int ata_pio48(uint16_t base, uint8_t type, uint16_t drive, uint32_t addr);
void init_hdd();
void drive_scan();
hdd_size_t drive_sectors(uint8_t devP, uint8_t controllerP);