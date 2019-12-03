#include <stdbool.h>
#include <libc.h>
#include <time.h>
#include <serial.h>
#include <stdio.h>
#include "hdd.h"
#include "hddw.h"
uint8_t *readBuffer;
uint8_t *writeBuffer;
uint16_t readOut[256];
uint16_t writeIn[256];
uint16_t emptySector[256];
uint32_t driveUseTick;
uint32_t lastSector = 0;
uint8_t current_drive = 1;

void init_hddw() {
    for(int i = 0; i < 256; i++)
    {
        emptySector[i] = 0x0;
    }
    readBuffer = (uint8_t *)kmalloc(512);
    writeBuffer = (uint8_t *)kmalloc(512);
}

void select_drive(uint8_t driveToSet) {
    if (driveToSet == 1) {
        if (mp == 0) {
            ata_drive = MASTER_DRIVE;
            ata_controler = PRIMARY_IDE;
            nodrives = 0;
            current_drive = driveToSet;
        } else if (mp48 == 0) {
            ata_drive = MASTER_DRIVE_PIO48;
            ata_controler = PRIMARY_IDE;
            ata_pio = 1;
            nodrives = 0;
            current_drive = driveToSet;
        } else {
            kprint("That drive is offline!\n");
        }
    } else if (driveToSet == 2) {
        if (ms == 0) {
            ata_drive = SLAVE_DRIVE;
            ata_controler = PRIMARY_IDE;
            nodrives = 0;
            current_drive = driveToSet;
        } else if (ms48 == 0) {
            ata_drive = SLAVE_DRIVE_PIO48;
            ata_controler = PRIMARY_IDE;
            ata_pio = 1;
            nodrives = 0;
            current_drive = driveToSet;
        } else {
            kprint("That drive is offline!\n");
        }
    } else if (driveToSet == 3) {
        if (sp == 0) {
            ata_drive = MASTER_DRIVE;
            ata_controler = SECONDARY_IDE;
            nodrives = 0;
            current_drive = driveToSet;
        } else if (sp48 == 0) {
            ata_drive = MASTER_DRIVE_PIO48;
            ata_controler = SECONDARY_IDE;
            ata_pio = 1;
            nodrives = 0;
            current_drive = driveToSet;
        } else {
            kprint("That drive is offline!\n");
        }
    } else if (driveToSet == 4) {
        if (ss == 0) {
            ata_drive = SLAVE_DRIVE;
            ata_controler = SECONDARY_IDE;
            ata_pio = 0;
            nodrives = 0;
            current_drive = driveToSet;
        } else if (ss48 == 0) {
            ata_drive = SLAVE_DRIVE_PIO48;
            ata_controler = SECONDARY_IDE;
            ata_pio = 1;
            nodrives = 0;
            current_drive = driveToSet;
        } else {
            kprint("That drive is offline!\n");
        }
    } else {
        kprint("Not a valid drive!\n");
    }
}

void read(uint32_t sector, uint32_t sector_high) {
    if (sector_high == 1) {

    }
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
    if (f == false && (tick-driveUseTick > 5000 || abs(sector-lastSector) > 50)) {
        // Sometimes the drive shuts off, so we need to wait for it to turn on
        wait(2000);
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
    driveUseTick = tick;
    lastSector = sector;
}

void readToBuffer(uint32_t sector) {
    uint8_t *ptr = readBuffer;
    read(sector, 0);
    //sprint("F: ");
    for (uint32_t i = 0; i < 256; i++)
    {
        uint16_t in = readOut[i];
        uint8_t s = (uint8_t)(in >> 8); // Default is f
        uint8_t f = (uint8_t)(in&0xff); // Default is s
        //sprint_uint(first);
        //sprint(" S: ");
        //sprint_uint(second);
        //sprint(" F: ");
        *ptr = s;
        ptr++;
        *ptr = f;
        ptr++;
    }
    //sprint("\n");
}

void writeFromBuffer(uint32_t sector, uint8_t badcheck) {
    uint8_t *ptr = writeBuffer;
    for (uint32_t i = 0; i < 256; i++)
    {
        uint8_t f = *ptr; // Default is s
        ptr++;
        uint8_t s = *ptr; // Default is f
        ptr++;
        uint16_t wd;
        wd = ((uint16_t)f << 8) | s;

        writeIn[i] = wd;
    }
    write(sector, badcheck);
}

void copy_sector(uint32_t sector1, uint32_t sector2) {
    ata_pio28(ata_controler, 1, ata_drive, sector1);
    ata_pio28(ata_controler, 2, ata_drive, sector2);
    clear_ata_buffer();
}

void write(uint32_t sector, uint8_t badcheck) {
    if ((tick-driveUseTick > 5000 || abs(sector-lastSector) > 50)) {
        read(sector, 0); // Start the drive
    }
    for(int i = 0; i < 256; i++)
    {
        ata_buffer[i] = writeIn[i];
    }
    if (ata_pio == 0) {
        ata_pio28(ata_controler, 2, ata_drive, sector);
    } else {
        ata_pio48(ata_controler, 2, ata_drive, sector);
    }
    uint32_t bad = 1;
    while (bad != 0) {
        if (ata_pio == 0) {
            ata_pio28(ata_controler, 2, ata_drive, sector);
        } else {
            ata_pio48(ata_controler, 2, ata_drive, sector);
        }
        read(sector, 0);
        bad = 0;
        for (int i = 0; i < 256; i++) {
            if (writeIn[i] != readOut[i]) {
                if (badcheck == 1) {
                    bad++;
                }
            }
        }
        if (badcheck == 1) {
            sprint("\n");
            sprint_uint(bad);
        }
    }
    clear_ata_buffer();
    for(int i = 0; i < 256; i++)
    {
        writeIn[i] = emptySector[i];
    }
    lastSector = sector;
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