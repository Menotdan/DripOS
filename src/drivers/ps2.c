#include "ps2.h"
#include "io/ports.h"
#include "drivers/tty/tty.h"
#include "fs/devfs/devfs.h"
#include "klibc/stdlib.h"
#include "proc/event.h"
#include "proc/scheduler.h"

event_t keyboard_event = 0;
lock_t ps2_buffer_lock = {0, 0, 0, 0};

uint8_t *kb_base_buffer = (void *) 0;
uint8_t *kb_read_buffer = (void *) 0;
uint8_t *kb_write_buffer = (void *) 0;
uint64_t kb_buffer_size = 0;

void wait_read() {
    while (!port_inb(0x64) & (1<<0)) asm volatile("pause"); // Wait for read
}

void keyboard_handler(int_reg_t *r) {
    trigger_event(&keyboard_event);
    UNUSED(r);
}

void ps2_keyboard_thread() {
    while (1) {
        await_event(&keyboard_event);
        uint8_t scan = port_inb(0x60); // Get the scancode
        lock(ps2_buffer_lock);
        if (!kb_base_buffer) {
            kb_base_buffer = krealloc(kb_base_buffer, kb_buffer_size + 256);
            kb_buffer_size += 256;
            kb_read_buffer = kb_base_buffer;
            kb_write_buffer = kb_base_buffer;
        }

        /* Allocate more space in buffer */
        if ((uint64_t) kb_write_buffer - (uint64_t) kb_base_buffer == kb_buffer_size) {
            uint64_t offset1 = (uint64_t) kb_write_buffer - (uint64_t) kb_base_buffer;
            uint64_t offset2 = (uint64_t) kb_read_buffer - (uint64_t) kb_base_buffer;
            kb_base_buffer = krealloc(kb_base_buffer, kb_buffer_size + 256);
            kb_buffer_size += 256;

            kb_read_buffer = kb_base_buffer + offset2;
            kb_write_buffer = kb_base_buffer + offset1;
        }

        *(kb_write_buffer++) = scan;
        unlock(ps2_buffer_lock);
    }
}

uint8_t read_kb_buffer() {
    uint8_t scancode = 0;

    lock(ps2_buffer_lock);
    if (kb_write_buffer == kb_base_buffer) {
        unlock(ps2_buffer_lock);
        return scancode;
    }

    scancode = *(kb_read_buffer++);
    if (kb_read_buffer == kb_write_buffer) {
        kb_read_buffer = kb_write_buffer = kb_base_buffer; // reset buffers
    }
    unlock(ps2_buffer_lock);
    
    return scancode;
}

int64_t mouse_x = 512;
int64_t mouse_y = 512;
void mouse_handler(int_reg_t *r) {

    uint8_t extra_bits = port_inb(0x60);
    uint8_t move_x = port_inb(0x60);
    uint8_t move_y = port_inb(0x60);

    int x = (int) move_x - (int) ((extra_bits << 4) & 0x100);
    int y = (int) move_y - (int) ((extra_bits << 3) & 0x100);

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

int ps2kb_read(int fd, void *buf, uint64_t count) {
    memset(buf, 0, count);
    uint8_t *buffer = buf;
    for (uint64_t i = 0; i < count; i++) {
        *(buffer++) = read_kb_buffer();
    }

    return 0;
}

void mouse_setup() {
    // port_outb(0x64, 0xA8); // Tell the PS/2 controller to enable port 2
    
    // /* Setup IRQ 12 */
    // port_outb(0x64, 0x20);
    // wait_read();
    // uint8_t status = port_inb(0x60);
    // kprintf("[PS/2] Status: %x\n", status);

    // status |= (1<<1);
    // status &= ~(1<<5);
    // port_outb(0x64, 0x60);
    // port_outb(0x60, status);

    // /* Enable mouse data reporting */
    // port_outb(0x64, 0xD4); // Tell the PS/2 controller to address the mouse
    // port_outb(0x60, 0xF4); // Send the enable data reporting command
    // wait_read();
    // uint8_t ack = port_inb(0x60); // Get the ack

    // if (ack != 0xFA) { // Check if its correct
    //     kprintf("[PS/2] Mouse setup failed.\n");
    // }

    // for (uint64_t i = 0; i < 256; i++)
    //     port_inb(0x60);

    vfs_ops_t ops = {devfs_open, 0, devfs_close, ps2kb_read, dummy_ops.write, dummy_ops.seek};
    register_device("keyboard", ops, (void *) 0);
    sprintf("registered /dev/keyboard\n");

    thread_t *ps2_kb = create_thread("PS/2 Keyboard handler", ps2_keyboard_thread, (uint64_t) kcalloc(TASK_STACK_SIZE) + TASK_STACK_SIZE, 0);
    add_new_child_thread(ps2_kb, 0);
}