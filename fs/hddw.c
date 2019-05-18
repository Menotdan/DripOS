#include "hdd.h"
#include "hddw.h"



void read(uint32_t sector) {
    ata_pio28(ata_controler, 1, ata_drive, sector); // Read disk into ata_buffer
    clear_ata_buffer();
}

void copy_sector(uint32_t sector1, uint32_t sector2) {
    ata_pio28(ata_controler, 1, ata_drive, sector1);
    ata_pio28(ata_controler, 2, ata_drive, sector2);
    clear_ata_buffer();
}

void writeFromBuffer(uint32_t sector) {
    ata_pio28(ata_controler, 2, ata_drive, sector);
    clear_ata_buffer();
}