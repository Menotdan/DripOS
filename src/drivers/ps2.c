#include "ps2.h"
#include "io/ports.h"
#include "drivers/tty/tty.h"
#include "klibc/stdlib.h"

void keyboard_handler(int_reg_t *r) {
    uint8_t scan = port_inb(0x60); // Get the scancode
    kprintf("\n[PS/2] Got scancode %x", (uint32_t) scan);
    UNUSED(r);
}