#include "hdd.h"
#include <stdint.h>
#include <stddef.h>
#include "../cpu/ports.h"
#include "../drivers/screen.h"
#include "../libc/string.h"
#include "../cpu/isr.h"
//ok
int ata_pio = 0;
uint32_t ata_drive;
uint32_t ata_controler;
void HD_READ(uint32_t sector, uint16_t *out_buf) {
    kprint_int(ata_drive);
    kprint("\n");
    kprint_int(ata_controler);
    kprint("\n");
    kprint_int(ata_pio);
    kprint("\n");
    int cycle;
    uint16_t buffer [256];
    uint8_t test;
    //kprint("Starting read by sending port info");
    port_byte_out (DRIVE_SELECT, ata_drive);
    cycle = 0;
    /*for (int i = 0; i <10000; i ++) {
        cycle = 0;
        test = (port_byte_in(ata_controler + 0x7));
        kprint_int(test);
        if ((test) == 0x08) {
            cycle = 1;
            kprint("set");
            break;
            kprint("what?");
        }    
    }*/
    port_byte_out (ata_controler + SECTOR_SET1, 0x00);
    port_byte_out (ata_controler + SECTOR_SET2, 0x01);
    port_byte_out (ata_controler + SECTOR_SET3, (unsigned char) sector);
    port_byte_out (ata_controler + SECTOR_SET4, (unsigned char) (sector >> 8));
    port_byte_out (ata_controler + SECTOR_SET5, (unsigned char) (sector >> 16));
    port_byte_out (ata_controler + READ_OR_WRITE, READ);
    for (int idx = 0; idx <256; idx ++)
    {
        buffer[idx] = port_word_in(ata_controler + DATA);
        out_buf[idx] = buffer[idx];
    }
    //kprint("Done reading");
    return;
}

void ata_detect() {
    //Detecting primary ata
    port_byte_out(0x1F3, 0x88);
    wait(2);
    if(port_byte_in(0x1F3)==0x88) {
        ata_controler=PRIMARY_BASE;

        port_byte_out(0x1F6, 0xA0);  //master
        wait(2);
        if(port_byte_in(0x1F7)==83) {  //pio48
            ata_pio=p48;
            ata_drive=PRIMARY;
            kprint("pmp48");
            return;
        }
        else {  //pio28
            ata_pio=p28;
            ata_drive=PRIMARY;
            kprint("pmp28");
            return;
        }

        port_byte_out(0x1F6, 0xB0);  //slave
        wait(2);
        if(port_byte_in(0x1F7)==83) {  //pio48
            ata_pio=p48;
            ata_drive=SLAVE;
            kprint("psp48");
            return;
        }
        else {  //pio28
            ata_pio=p28;
            ata_drive=SLAVE;
            kprint("psp28");
            return;
        }
    }

    //Detecting secondary ata
    port_byte_out(0x173, 0x88);
    wait(2);
    if(port_byte_in(0x173)==0x88) {
        ata_controler=SLAVE_BASE;

        port_byte_out(0x176, 0xA0);  //master
        wait(2);
        if(port_byte_in(0x177)==83) {  //pio48
            ata_pio=p48;
            ata_drive=PRIMARY;
            kprint("smp48");
            return;
        }
        else {  //pio28
            ata_pio=p28;
            ata_drive=PRIMARY;
            kprint("smp28");
            return;
        }

        port_byte_out(0x176, 0xB0);  //slave
        wait(2);
        if(port_byte_in(0x177)==83) {  //pio48
            ata_pio=p48;
            ata_drive=SLAVE;
            kprint("ssp48");
            return;
        }
        else {  //pio28
            ata_pio=p28;
            ata_drive=SLAVE;
            kprint("ssp28");
            return;
        }
    }
}