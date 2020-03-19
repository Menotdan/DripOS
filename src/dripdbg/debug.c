#include "debug.h"
#include "protocol.h"
#include "drivers/serial.h"
#include "drivers/tty/tty.h"
#include "proc/scheduler.h"
#include "klibc/string.h"
#include "klibc/stdlib.h"
#include "klibc/math.h"

// GDB remote in kernel bad :meme:

void dripdbg_send(char *msg) {
    char final_msg[strlen(msg) + 5 + 1];
    final_msg[0] = '\0';

    strcat(final_msg, msg);
    strcat(final_msg, "#$^&e");
    serial_write_buf((uint8_t *) final_msg, strlen(msg) + 5);
}

char *dripdbg_add_list(char *dst, char *new, uint64_t *cur_buf_size) {
    char *buf = dst;

    if (strlen(buf) + strlen(new) + 3 > *cur_buf_size) {
        buf = krealloc(buf, strlen(buf) + strlen(new) + 3);
        *cur_buf_size = strlen(buf) + strlen(new) + 3;
    }

    strcat(buf, "+=");
    strcat(buf, new);

    return buf;
}

char *dripdbg_add_list_int(char *dst, uint64_t num, uint64_t *cur_buf_size) {
    char buf[30];
    utoa(num, buf);
    return dripdbg_add_list(dst, buf, cur_buf_size);
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
            } else if (buffer[0] == 't') {
                // Send a list of thread names
                char *data = kcalloc(1);
                for (uint64_t i = 0; i < get_thread_list_size(); i++) {
                    if (get_thread_elem(i)) {
                        task_t *task = get_thread_elem(i);
                        data = krealloc(data, strlen(data) + 3 + strlen(task->name));
                        strcat(data, "+=");
                        strcat(data, task->name);
                    }
                }
                dripdbg_send(data);
                kfree(data);
            } else if (buffer[0] == 'i') {
                // Send a list of task ids
                char *data = kcalloc(1);
                for (uint64_t i = 0; i < get_thread_list_size(); i++) {
                    if (get_thread_elem(i)) {
                        task_t *task = get_thread_elem(i);
                        char id[30];
                        utoa(task->tid, id);
                        data = krealloc(data, strlen(data) + 3 + strlen(id));
                        strcat(data, "+=");
                        strcat(data, id);
                    }
                }
                dripdbg_send(data);
                kfree(data);
            } else if (buffer[0] == 'I') {
                // Send info about a thread
                char *data = kcalloc(1);
                uint64_t cur_buf_size = 1;
                uint64_t tid = atou(buffer + 1);
                task_t *task = get_thread_elem(tid);

                // Get the data together
                data = dripdbg_add_list_int(data, task->regs.rax, &cur_buf_size);
                data = dripdbg_add_list_int(data, task->regs.rbx, &cur_buf_size);
                data = dripdbg_add_list_int(data, task->regs.rcx, &cur_buf_size);
                data = dripdbg_add_list_int(data, task->regs.rdx, &cur_buf_size);
                data = dripdbg_add_list_int(data, task->regs.rdi, &cur_buf_size);
                data = dripdbg_add_list_int(data, task->regs.rsi, &cur_buf_size);
                data = dripdbg_add_list_int(data, task->regs.rbp, &cur_buf_size);
                data = dripdbg_add_list_int(data, task->regs.r8, &cur_buf_size);
                data = dripdbg_add_list_int(data, task->regs.r9, &cur_buf_size);
                data = dripdbg_add_list_int(data, task->regs.r10, &cur_buf_size);
                data = dripdbg_add_list_int(data, task->regs.r11, &cur_buf_size);
                data = dripdbg_add_list_int(data, task->regs.r12, &cur_buf_size);
                data = dripdbg_add_list_int(data, task->regs.r13, &cur_buf_size);
                data = dripdbg_add_list_int(data, task->regs.r14, &cur_buf_size);
                data = dripdbg_add_list_int(data, task->regs.r15, &cur_buf_size);
                data = dripdbg_add_list_int(data, task->regs.rsp, &cur_buf_size);
                data = dripdbg_add_list_int(data, task->regs.rip, &cur_buf_size);
                data = dripdbg_add_list_int(data, task->regs.rflags, &cur_buf_size);
                data = dripdbg_add_list_int(data, task->regs.cs, &cur_buf_size);
                data = dripdbg_add_list_int(data, task->regs.ss, &cur_buf_size);
                data = dripdbg_add_list_int(data, task->regs.cr3, &cur_buf_size);

                dripdbg_send(data);
                kfree(data);
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