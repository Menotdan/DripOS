#include "ps2.h"
#include "../cpu/task.h"

uint16_t buffer_index = 0;
uint8_t err = 0;

void mouse_handler(registers_t *r) {
    // TODO: Make mouse handler
    sprint("\nPS/2: Got mouse");
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

void keyboard_handler(registers_t *r) {
    /* Store scancode */
    char is_mouse = (port_byte_in(0x64) & 0x20) >> 5;
    if (is_mouse == 0) {
        char scan = port_byte_in(0x60);
        //sprintf("\nGot key %x", ((uint32_t) scan) & 0xff);
        if (loaded) {
            if (scan == 0xf) {
                sprint_tasks();
            }
            *(get_focused_task()->scancode_buffer + get_focused_task()->scancode_buffer_pos) = scan;
            get_focused_task()->scancode_buffer_pos++;
        }
    } else {
        mouse_handler(r);
    }
    UNUSED(r);
}

char get_scancode() {
    char ret;
    if (running_task->scancode_buffer_pos > 0) {
        ret = *(running_task->scancode_buffer + running_task->scancode_buffer_pos - 1);
        *(running_task->scancode_buffer + running_task->scancode_buffer_pos - 1) = 0;
        running_task->scancode_buffer_pos--;
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