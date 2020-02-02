#include "serial.h"
#include "../cpu/timer.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define PORT 0x3f8 /* COM1 */

int is_transmit_empty() {
    return port_byte_in(PORT + 5) & 0x20;
}

void init_serial() {
    port_byte_out(PORT + 1, 0x00); // Disable all interrupts
    port_byte_out(PORT + 3, 0x80); // Enable DLAB (set baud rate divisor)
    port_byte_out(PORT + 0, 0x03); // Set divisor to 3 (lo byte) 38400 baud
    port_byte_out(PORT + 1, 0x00); //                  (hi byte)
    port_byte_out(PORT + 3, 0x03); // 8 bits, no parity, one stop bit
    port_byte_out(PORT + 2, 0xC7); // Enable FIFO, clear them, with 14-byte threshold
    port_byte_out(PORT + 4, 0x0B); // IRQs enabled, RTS/DSR set
}

void write_serial(char a) {
    while (is_transmit_empty() == 0);
    port_byte_out(PORT, a);
}

void sprint(char *message) {
    int i = 0;
    while (message[i] != 0) {
        write_serial(message[i++]);
    }
}

void sprint_int(int32_t num) {
    char toprint[33];
    int_to_ascii(num, toprint);
    sprint(toprint);
}

void sprint_int64(int64_t num) {
    char toprint[33];
    int64_to_ascii(num, toprint);
    sprint(toprint);
}

void sprint_uint(unsigned int num) {
    char toprint[33];
    uint_to_ascii(num, toprint);
    sprint(toprint);
}

void sprint_uint64(uint64_t num) {
    char toprint[64];
    uint64_to_ascii(num, toprint);
    sprint(toprint);
}

void sprint_hex(uint64_t num) {
    char toprint[20];
    htoa(num, toprint);
    sprint(toprint);
}

void sprintf(char *message, ...) {
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
                        sprint_hex(va_arg(format_list, uint64_t));
                    } else {
                        sprint_hex(va_arg(format_list, uint32_t));
                    }
                    break;
                case 'd':
                    if (big) {
                        sprint_int64(va_arg(format_list, int64_t));
                    } else {
                        sprint_int(va_arg(format_list, int32_t));
                    }
                    break;
                case 'u':
                    if (big) {
                        sprint_uint64(va_arg(format_list, uint64_t));
                    } else {
                        sprint_uint(va_arg(format_list, uint32_t));
                    }
                    break;
                case 's':
                    sprint(va_arg(format_list, char *));
                    break;
                default:
                    break;
            }
        } else {
            write_serial(message[index]);
        }
        index++;
    }

    va_end(format_list);
}

void sprintd(char *message) {
    sprint("[");
    sprint_uint(tick);
    sprint(" ticks] [DripOS]: ");
    sprint(message);
    sprint("\n");
}