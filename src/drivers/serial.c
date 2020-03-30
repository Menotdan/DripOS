#include "serial.h"
#include <stdarg.h>
#include "io/ports.h"
#include "klibc/string.h"
#include "klibc/lock.h"
#include "klibc/stdlib.h"

lock_t serial_print_lock = {0, 0, 0};

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
#ifdef DEBUG
    serial_transmit_delay(com_port); // Wait for buffer to be ready
    port_outb(com_port, data); // Send the data to the COM port
#else
    (void) data;
    (void) com_port;
#endif
}

uint8_t read_buffer() {
    if (port_inb(COM1 + 5) & 1) return port_inb(COM1);
    return 0;
}

uint8_t is_data() {
    return port_inb(COM1 + 5) & 1;
}

void serial_write_buf(uint8_t *buffer, uint64_t count) {
    for (uint64_t i = 0; i < count; i++) {
        serial_transmit_delay(COM1); // Wait for buffer to be ready
        port_outb(COM1, buffer[i]); // Send the data to the COM port
    }
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

void sprintf(char *message, ...) {
#ifdef DEBUG
    //lock(&serial_print_lock);
    va_list format_list;
    uint64_t index = 0;
    uint8_t big = 0;

    va_start(format_list, message);

    while (message[index]) {
        if (message[index] == '%') {
            index++;
            if (message[index] == 'l') {
                index++;
                big = 1;
            }
            switch (message[index]) {
                case 'x':
                    if (big) {
                        uint64_t data = va_arg(format_list, uint64_t);
                        char data_buf[32];
                        htoa(data, data_buf);
                        sprint(data_buf);
                    } else {
                        uint32_t data = va_arg(format_list, uint32_t);
                        char data_buf[32];
                        htoa((uint64_t) data, data_buf);
                        sprint(data_buf);
                    }
                    break;
                case 'd':
                    if (big) {
                        int64_t data = va_arg(format_list, int64_t);
                        char data_buf[32];
                        itoa(data, data_buf);
                        sprint(data_buf);
                    } else {
                        int32_t data = va_arg(format_list, int32_t);
                        char data_buf[32];
                        itoa((int64_t) data, data_buf);
                        sprint(data_buf);
                    }
                    break;
                case 'u':
                    if (big) {
                        uint64_t data = va_arg(format_list, uint64_t);
                        char data_buf[32];
                        utoa(data, data_buf);
                        sprint(data_buf);
                    } else {
                        uint32_t data = va_arg(format_list, uint32_t);
                        char data_buf[32];
                        utoa((uint32_t) data, data_buf);
                        sprint(data_buf);
                    }
                    break;
                case 's':
                    sprint(va_arg(format_list, char *));
                    break;
                default:
                    break;
            }
        } else {
            write_serial(message[index], COM1);
        }
        index++;
    }

    va_end(format_list);
    //unlock(&serial_print_lock);
    //yield();
#else
    (void) message;
#endif
}