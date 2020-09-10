#ifndef SERIAL_H
#define SERIAL_H
#include <stdint.h>

#define COM1 0x3f8
#define SERIAL_DLAB (1<<7) // The bit that you send to port + 3 to enable setting the baud
                            // rate divisor
#define SERIAL_RATE_38400_LO 3
#define SERIAL_RATE_38400_HI 0
#define SERIAL_BUFFER_EMPTY (1<<5) // The bit from port + 5 to tell if the buffer is empty

void init_serial(uint16_t com_port);
void write_serial(char data, uint16_t com_port);
void sprint_com_port(char *s, uint16_t com_port);
void sprint(char *s);
void sprintf(char *message, ...);

void serial_write_buf(uint8_t *buffer, uint64_t count);
uint8_t read_buffer();
uint8_t is_data();

#endif