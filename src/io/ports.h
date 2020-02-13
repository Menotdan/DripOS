#ifndef PORTS_H
#define PORTS_H
#include <stdint.h>

// Port I/O functions

uint8_t port_inb(uint16_t port);
uint16_t port_inw(uint16_t port);
uint32_t port_ind(uint16_t port);

void port_outb(uint16_t port, uint8_t data);
void port_outw(uint16_t port, uint16_t data);
void port_outd(uint16_t port, uint32_t data);

#endif