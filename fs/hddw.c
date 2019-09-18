#include <stdbool.h>
#include <libc.h>
#include "hdd.h"
#include "hddw.h"
uint8_t *readBuffer;
uint16_t readOut[256];
uint16_t writeIn[256];
uint16_t emptySector[256];

void init_hddw() {
    for(int i = 0; i < 256; i++)
    {
        emptySector[i] = 0x0;
    }
    readBuffer = (uint8_t *)kmalloc(512);
}

void read(uint32_t sector) {
    if (ata_pio == 0) {
        ata_pio28(ata_controler, 1, ata_drive, sector); // Read disk into ata_buffer
    } else {
        ata_pio48(ata_controler, 1, ata_drive, sector); // Read disk into ata_buffer
    }
    bool f = false;
    for(int i = 0; i < 256; i++)
    {
        if (ata_buffer[i] != 0) {
            f = true;
        }
        readOut[i] = ata_buffer[i];
    }
    clear_ata_buffer();
    if (f == false) {
        // Sometimes the drive shuts off, so we need to wait for it to turn on
        int l = 0;
        for (int i = 0; i < 10000; i++)
        {
            l++; // Delay
        }
        
        if (ata_pio == 0) {
            ata_pio28(ata_controler, 1, ata_drive, sector); // Read disk into ata_buffer
        } else {
            ata_pio48(ata_controler, 1, ata_drive, sector); // Read disk into ata_buffer
        }

        for(int i = 0; i < 256; i++)
        {
            readOut[i] = ata_buffer[i];
        }
    }
}

void readToBuffer(uint32_t sector) {
    uint8_t *ptr = readBuffer;
    read(sector);
    for (uint32_t i = 0; i < 256; i++)
    {
        uint16_t in = readOut[i];
        uint8_t first = (uint8_t)in >> 8;
        uint8_t second = (uint8_t)in&0xff;
        *ptr = first;
        ptr++;
        *ptr = sector;
        ptr++;
    }
    
}

void copy_sector(uint32_t sector1, uint32_t sector2) {
    ata_pio28(ata_controler, 1, ata_drive, sector1);
    ata_pio28(ata_controler, 2, ata_drive, sector2);
    clear_ata_buffer();
}

void writeFromBuffer(uint32_t sector) {
    for(int i = 0; i < 256; i++)
    {
        ata_buffer[i] = writeIn[i];
    }
    if (ata_pio == 0) {
        ata_pio28(ata_controler, 2, ata_drive, sector);
    } else {
        ata_pio48(ata_controler, 2, ata_drive, sector);
    }
    clear_ata_buffer();
    for(int i = 0; i < 256; i++)
    {
        writeIn[i] = emptySector[i];
    }
}

void clear_sector(uint32_t sector) {
    for(int i = 0; i < 256; i++)
    {
        ata_buffer[i] = emptySector[i];
    }
    if (ata_pio == 0) {
        ata_pio28(ata_controler, 2, ata_drive, sector);
    } else {
        ata_pio48(ata_controler, 2, ata_drive, sector);
    }
}