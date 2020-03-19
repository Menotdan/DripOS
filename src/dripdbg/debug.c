#include "debug.h"
#include "protocol.h"
#include "drivers/serial.h"
#include "drivers/tty/tty.h"
#include "proc/scheduler.h"
#include "klibc/string.h"
#include "klibc/stdlib.h"
#include "klibc/math.h"

void dripdbg_send(char *msg) {
    char final_msg[strlen(msg) + 5 + 1];
    final_msg[0] = '\0';

    strcat(final_msg, msg);
    strcat(final_msg, "#$^&e");
    serial_write_buf((uint8_t *) final_msg, strlen(msg) + 5);
}

void debug_worker() {
    while (1) {
        uint64_t buffer_size = 10;
        char *buffer = kcalloc(10);
        uint64_t buffer_index = 0;
        uint8_t got_data = 0;

        if (is_data()) {
            got_data = 1;
            while (1) {
                if (is_data()) {
                    char data = read_buffer();
                    buffer[buffer_index++] = data;

                    if (buffer_index == buffer_size) buffer = krealloc(buffer, buffer_size + 10);
                    buffer_size += 10;

                    char *check_eom = buffer + (buffer_index - 5);
                    buffer[buffer_index] = '\0';
                    if (strcmp(check_eom, "#$^&e") == 0 && buffer_index >= 6) {
                        *check_eom = '\0';
                        break;
                    }
                }
            }
        }
        yield();

        // Parse data
        if (got_data) {
            buffer[buffer_index] = '\0';
            if (buffer[0] == 'p') {
                // Print everything after the 'p' command
                char *print_dat = buffer + 1;
                kprintf("%s", print_dat);
            } else if (buffer[0] == 'h') {
                // Send back "hello world"
                dripdbg_send("hello world");
            } else {
                kprintf("\n[DripDBG] Unknown message: '%s'", buffer);
            }
        }

        kfree(buffer);
    }
}

void setup_drip_dgb() {
    new_kernel_process("DripOS Debugger", debug_worker);
}