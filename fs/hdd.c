#include "hdd.h"
#include <stdint.h>
#include <stddef.h>
#include "../cpu/ports.h"
#include "../drivers/screen.h"
#include "../libc/string.h"
//ok
void HD_READ(uint32_t sector, uint32_t DRIVE, uint16_t *out_buf) {
    int cycle;
    uint16_t buffer [256];
    //kprint("Starting read by sending port info");
    port_byte_out (DRIVE_SELECT, DRIVE);
    port_byte_out (SECTOR_SET1, 0x00);
    port_byte_out (SECTOR_SET2, 0x01);
    port_byte_out (SECTOR_SET3, (unsigned char) sector);
    port_byte_out (SECTOR_SET4, (unsigned char) (sector >> 8));
    port_byte_out (SECTOR_SET5, (unsigned char) (sector >> 16));
    port_byte_out (READ_OR_WRITE, READ);
    cycle = 0;
    for (int i = 0; i <10000; i ++) {
        if ((port_byte_in(0x1F7) & 0x08) == 0x08) {
            cycle = 1;
            break;    
        }    
    }
    if (cycle == 0) {// The DRQ bit has not been set, the call has collapsed, so we have to end the method
        return;
    }
    for (int idx = 0; idx <256; idx ++)
    {
        buffer[idx] = port_word_in(0x1F0);
        kprint_int(buffer[idx]);
        out_buf[idx] = buffer[idx];
    }
    //kprint("Done reading");
    return;
}