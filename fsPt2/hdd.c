#include "hdd.h"
#include <stdint.h>
#include <stddef.h>
#include "../cpuPt2/ports.h"
#include "../driversPt2/screen.h"
#include "../libcPt2/string.h"
#include "../cpuPt2/isr.h"
//ok
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

#define ATA_SR_BSY     0x80    // Busy
#define ATA_SR_DRQ     0x08    // Data request ready
#define ATA_SR_ERR     0x01    // Error

#define ATA_READ 1
#define ATA_WRITE 2

#define ATA_PIO28 0
#define ATA_PIO48 1
int ata_pio = 0;
uint16_t ata_drive = MASTER_DRIVE;
uint32_t ata_controler = PRIMARY_IDE;
uint16_t ata_buffer[256];

void clear_ata_buffer() {
    for(int i=0; i<256; i++) {
        ata_buffer[i]=0;
    }
}

int ata_pio28(uint16_t base, uint8_t type, uint16_t drive, uint32_t addr) {
    int cycle=0;
    port_byte_out(base+ATA_PORT_DRV, drive);
    //PIO28
    port_byte_out(base+ATA_PORT_FEATURES, 0x00);
    port_byte_out(base+ATA_PORT_SCT_COUNT, 0x01);
    port_byte_out(base+ATA_PORT_SCT_NUMBER, (unsigned char)addr);
    port_byte_out(base+ATA_PORT_CYL_LOW, (unsigned char)(addr >> 8));
    port_byte_out(base+ATA_PORT_CYL_HIGH, (unsigned char)(addr >> 16));
    //type
    if(type==ATA_READ) {
    port_byte_out(base+ATA_PORT_COMMAND, 0x20);  // Send command
    }
    else {
    port_byte_out(base+ATA_PORT_COMMAND, 0x30);
    }

    //wait for BSY clear and DRQ set
    cycle=0;
    for(int i=0; i<1000; i++) {
        port_byte_in(base+ATA_PORT_ALT_STATUS);  //Delay
        port_byte_in(base+ATA_PORT_ALT_STATUS);
        port_byte_in(base+ATA_PORT_ALT_STATUS);
        port_byte_in(base+ATA_PORT_ALT_STATUS);
        if( (port_byte_in(base+ATA_PORT_ALT_STATUS) & 0x88)==0x08 ) {  //drq is set
            cycle=1;
            break;    
        }    
    }
    if(cycle==0) {
        port_byte_in(base+ATA_PORT_ALT_STATUS); //Delay
        port_byte_in(base+ATA_PORT_ALT_STATUS);
        port_byte_in(base+ATA_PORT_ALT_STATUS);
        port_byte_in(base+ATA_PORT_ALT_STATUS);
        if( (port_byte_in(base+ATA_PORT_ALT_STATUS) & 0x01)==0x01 ) {
            kprint("Bad block! Bad Cycle! ");
        } 
        return 0;
    }

    if( (port_byte_in(base+ATA_PORT_ALT_STATUS) & 0x01)==0x01 ) {
        kprint("Bad block! ");
    }

    for (int idx = 0; idx < 256; idx++)
    {
        if(type==ATA_READ) {
            ata_buffer[idx] = port_word_in(base + ATA_PORT_DATA);
        }
        else {
            port_word_out(base + ATA_PORT_DATA, ata_buffer[idx]);
        }
    }

    return 1;
}

