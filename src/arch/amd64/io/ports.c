#include "ports.h"

uint8_t port_inb(uint16_t port) {
    uint8_t ret;
    __asm__("in %%dx, %%al" : "=a"(ret) : "d"(port));
    return ret;
}

uint16_t port_inw(uint16_t port) {
    uint16_t ret;
    __asm__("in %%dx, %%ax" : "=a"(ret) : "d"(port));
    return ret;
}

uint32_t port_ind(uint16_t port) {
    uint32_t ret;
    __asm__("in %%dx, %%eax" : "=a"(ret) : "d"(port));
    return ret;
}

void port_outb(uint16_t port, uint8_t data) {
    __asm__("out %%al, %%dx" : : "a"(data), "d"(port));
}

void port_outw(uint16_t port, uint16_t data) {
    __asm__("out %%ax, %%dx" : : "a"(data), "d"(port));
}

void port_outd(uint16_t port, uint32_t data) {
    __asm__("out %%eax, %%dx" : : "a"(data), "d"(port));
}