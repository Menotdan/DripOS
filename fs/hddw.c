#include "hdd.h"
#include "hddw.h"
uint16_t readOut[256];
uint16_t writeIn[256];
uint16_t emptySector[256];

void empty_sector() {
    for(int i = 0; i < 256; i++)
    {
        emptySector[i] = 0x0;
    }
    
}

void read(uint32_t sector) {
    ata_pio28(ata_controler, 1, ata_drive, sector); // Read disk into ata_buffer
    for(int i = 0; i < 256; i++)
    {
        readOut[i] = ata_buffer[i];
    }
    clear_ata_buffer();
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
    ata_pio28(ata_controler, 2, ata_drive, sector);
    clear_ata_buffer();
}

void clear_sector(uint32_t sector) {
    for(int i = 0; i < 256; i++)
    {
        ata_buffer[i] = emptySector[i];
    }
    ata_pio28(ata_controler, 2, ata_drive, sector);
}