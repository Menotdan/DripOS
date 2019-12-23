/*
    ps2.c
    Copyright Menotdan 2018-2019

    Keypress Handler

    MIT License
*/

#include <drivers/ps2.h>

char scancode_buffer[2000];
char scancode = 0;
uint16_t buffer_index = 0;
uint8_t err = 0;

void mouse_handler(registers_t* r) {
    // TODO: Make mouse handler
    char mouse_data1 = port_byte_in(0x60);
    char mouse_data2 = port_byte_in(0x60); // X
    char mouse_data3 = port_byte_in(0x60); // Y
    sprint("\nX: ");
    sprint_uint((uint8_t)mouse_data2);
    sprint("\nY: ");
    sprint_uint((uint8_t)mouse_data3);

    UNUSED(r);
    UNUSED(mouse_data1);
}

void keyboard_handler(registers_t* r) {
    /* Store scancode */
    char is_mouse = (port_byte_in(0x64) & 0x20) >> 5;
    if (is_mouse == 0) {
        char scan = port_byte_in(0x60);
        if (scancode == 0) {
            scancode = scan;
        } else {
            scancode_buffer[buffer_index] = scan;
            buffer_index++;
        }
    } else {
        mouse_handler(r);
    }
    UNUSED(r);
}

char get_scancode() {
    char ret;
    if (buffer_index > 0) {
        ret = scancode_buffer[buffer_index-1];
        scancode_buffer[buffer_index-1] = 0;
        buffer_index--;
    } else if (scancode != 0) {
        ret = scancode;
        scancode = 0;
    } else {
        err = 1;
        ret = 0;
    }
    return ret;
}

/* PS/2 init function */
void init_ps2() {
    register_interrupt_handler(IRQ1, keyboard_handler); // Setup keyboard IRQ
    register_interrupt_handler(IRQ12, mouse_handler); // Setup mouse IRQ
}