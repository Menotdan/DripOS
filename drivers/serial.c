#include "serial.h"
#include "../libc/string.h"
#include "../cpu/timer.h"
#include <stdio.h>

#define PORT 0x3f8   /* COM1 */
 
void init_serial() {
   port_byte_out(PORT + 1, 0x00);    // Disable all interrupts
   port_byte_out(PORT + 3, 0x80);    // Enable DLAB (set baud rate divisor)
   port_byte_out(PORT + 0, 0x03);    // Set divisor to 3 (lo byte) 38400 baud
   port_byte_out(PORT + 1, 0x00);    //                  (hi byte)
   port_byte_out(PORT + 3, 0x03);    // 8 bits, no parity, one stop bit
   port_byte_out(PORT + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
   port_byte_out(PORT + 4, 0x0B);    // IRQs enabled, RTS/DSR set
}


int is_transmit_empty() {
   return port_byte_in(PORT + 5) & 0x20;
}
 
void write_serial(char a) {
   while (is_transmit_empty() == 0);
 
   port_byte_out(PORT,a);
}

void sprint(char *message) {
    int i = 0;
    while (message[i] != 0) {
        write_serial(message[i++]);
    }
}

void sprintd(char *message) {
    sprint("[");
    sprint_uint(tick);
    sprint(" ticks] [DripOS]: ");
    sprint(message);
    sprint("\n");
}