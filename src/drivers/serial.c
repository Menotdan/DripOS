#include "serial.h"
#include "io/ports.h"

void init_serial(uint16_t com_port) {
    port_outb(com_port + 1, 0); // Disable interrupts for this COM port
    port_outb(com_port + 3, SERIAL_DLAB); // Set the DLAB bit (for setting baud rate divisor)
    port_outb(com_port + 0, SERIAL_RATE_38400_LO); // Set baud rate to 38400 (lo)
    port_outb(com_port + 1, SERIAL_RATE_38400_HI); //                        (hi)
    port_outb(com_port + 3, 3); // 8 bits at a time, no parity (No checking validity of data)
    port_outb(com_port + 2, 0xC7); // Enable FIFO, clear them, with 14-byte threshold
    port_outb(com_port + 4, 0x0B); // IRQs enabled, RTS/DSR set
}

void serial_transmit_delay(uint16_t com_port) {
    /* Hang while we wait for the buffer to be empty */
    while (!(port_inb(com_port + 5) & SERIAL_BUFFER_EMPTY));
}

void write_serial(char data, uint16_t com_port) {
    serial_transmit_delay(com_port); // Wait for buffer to be ready
    port_outb(com_port, data); // Send the data to the COM port
}

// Print, but you can select a port
void sprint_com_port(char *s, uint16_t com_port) {
    while (*s != '\0') {
        write_serial(*s, com_port);
        s++;
    }
}

// Print on COM1 by default
void sprint(char *s) {
    sprint_com_port(s, COM1);
}