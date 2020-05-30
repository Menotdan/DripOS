#include "ps2.h"
#include "io/ports.h"
#include "drivers/tty/tty.h"
#include "klibc/stdlib.h"

void wait_read() {
    while (!port_inb(0x64) & (1<<0)) asm volatile("pause"); // Wait for read
}

void keyboard_handler(int_reg_t *r) {
    uint8_t scan = port_inb(0x60); // Get the scancode
    sprintf("Keyboard: %x\n", scan);
    UNUSED(r);
    UNUSED(scan);
}

int64_t mouse_x = 512;
int64_t mouse_y = 512;
void mouse_handler(int_reg_t *r) {

    uint8_t extra_bits = port_inb(0x60);
    uint8_t move_x = port_inb(0x60);
    uint8_t move_y = port_inb(0x60);

    int x = (int) move_x - (int) ((extra_bits << 4) & 0x100);
    int y = (int) move_y - (int) ((extra_bits << 3) & 0x100);

    //sprintf("Byte 0: %x\nByte 1: %x\nByte 2: %x\n", extra_bits, move_x, move_y);
    //sprintf("Mouse: %d, %d\n", x, y);

    if (!(x == 65 && y == 65) && !(x == 97 && y == -159)) {
        mouse_x += x / MOUSE_SENSITIVITY;
        mouse_y -= y / MOUSE_SENSITIVITY;
        if (mouse_x >= (int64_t) vesa_display_info.width) {
            mouse_x -= x / MOUSE_SENSITIVITY;
        }
        if (mouse_y >= (int64_t) vesa_display_info.height) {
            mouse_y += y / MOUSE_SENSITIVITY;
        }
        if (mouse_x < 0) {
            mouse_x -= x / MOUSE_SENSITIVITY;
        }
        if (mouse_y < 0) {
            mouse_y += y / MOUSE_SENSITIVITY;
        }
    }

    //color_t color = {0, 255, 255};
    //put_pixel(mouse_x, mouse_y, color);
    //flip_buffers();
    UNUSED(r);
}

void mouse_setup() {
    port_outb(0x64, 0xA8); // Tell the PS/2 controller to enable port 2
    
    /* Setup IRQ 12 */
    port_outb(0x64, 0x20);
    wait_read();
    uint8_t status = port_inb(0x60);
    kprintf("[PS/2] Status: %x\n", status);

    status |= (1<<1);
    status &= ~(1<<5);
    port_outb(0x64, 0x60);
    port_outb(0x60, status);

    /* Enable mouse data reporting */
    port_outb(0x64, 0xD4); // Tell the PS/2 controller to address the mouse
    port_outb(0x60, 0xF4); // Send the enable data reporting command
    wait_read();
    uint8_t ack = port_inb(0x60); // Get the ack

    if (ack != 0xFA) { // Check if its correct
        kprintf("[PS/2] Mouse setup failed.\n");
    }

    port_inb(0x60);
    port_inb(0x60);
}