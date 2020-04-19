#include "tty.h"
#include "klibc/font.h"
#include "klibc/string.h"
#include "klibc/lock.h"
#include "klibc/stdlib.h"
#include "proc/scheduler.h"
#include <stdarg.h>

#include "drivers/serial.h"

color_t default_fg = {248, 248, 248};
color_t default_bg = {56, 56, 56};
tty_t base_tty;

int tty_dev_write(fd_entry_t *fd_data, void *buf, uint64_t count) {
    (void) fd_data;
    lock(base_tty.tty_lock);

    char *char_buf = buf;
    for (uint64_t i = 0; i < count; i++) {
        tty_out(char_buf[i], &base_tty);
    }

    flip_buffers();

    unlock(base_tty.tty_lock);
    return 0;
}

int tty_dev_read(fd_entry_t *fd_data, void *buf, uint64_t count) {
    (void) fd_data;

    char *char_buf = buf;
    for (uint64_t i = 0; i < count; i++) {
        char_buf[i] = tty_get_char(&base_tty);
    }

    return 0;
}

void tty_init(tty_t *tty, uint64_t font_width, uint64_t font_height) {
    tty->fg = default_fg;
    tty->bg = default_bg;
    tty->c_pos_x = 0;
    tty->c_pos_y = 0;
    tty->rows = vesa_display_info.height / font_height;
    tty->cols = vesa_display_info.width / font_width;
    tty->font = (uint8_t *) font8x8_basic;
    tty->tty_lock.current_holder = 0;
    tty->tty_lock.lock_dat = 0;
    tty->kb_in_buffer = kcalloc(4096);
    tty->kb_in_buffer_index = 0;
    tty_clear(tty);
}

void tty_in(char c, tty_t *tty) {
    lock(tty->tty_lock);
    tty->kb_in_buffer[tty->kb_in_buffer_index++] = c;
    unlock(tty->tty_lock);
}

char tty_get_char(tty_t *tty) {
    lock(tty->tty_lock);
    if (tty->kb_in_buffer_index == 0) {
        unlock(tty->tty_lock);
        return 0; // No code
    }

    /* Grab the code and return it */
    char ret = tty->kb_in_buffer[tty->kb_in_buffer_index - 1];
    tty->kb_in_buffer_index--;

    unlock(tty->tty_lock);
    return ret;
}

void tty_out(char c, tty_t *tty) {
    if (c == '\n') {
        tty->c_pos_y += 1;
        tty->c_pos_x = 0;
    } else {
        render_font((uint8_t (*)[8]) tty->font, c, tty->c_pos_x * 8, tty->c_pos_y * 8, tty->fg, tty->bg);
        tty->c_pos_x++;
        if (tty->c_pos_x == tty->cols) {
            tty->c_pos_y += 1;
            tty->c_pos_x = 0;
        }
    }

    if (tty->c_pos_y == tty->rows) {
        vesa_scroll(8, tty->bg);
        tty->c_pos_y--;
    }
}

void tty_seek_no_lock(uint64_t x, uint64_t y, tty_t *tty) {
    if (x < tty->cols && y < tty->rows) {
        tty->c_pos_x = x;
        tty->c_pos_y = y;
    }
}

void tty_clear(tty_t *tty) {
    lock(tty->tty_lock);
    fill_screen(tty->bg);
    tty_seek_no_lock(0, 0, tty); // Since we already have the lock
    unlock(tty->tty_lock);
}

void kprint(char *s) {
    while (*s != '\0') {
        tty_out(*s++, &base_tty);
    }
}

void kprintf(char *message, ...) {
    lock(base_tty.tty_lock);
    va_list format_list;
    uint64_t index = 0;
    uint8_t big = 0;

    va_start(format_list, message);

    while (message[index]) {
        if (message[index] == '%') {
            index++;
            if (message[index] == 'l') {
                index++;
                big = 1;
            }
            switch (message[index]) {
                case 'x':
                    if (big) {
                        uint64_t data = va_arg(format_list, uint64_t);
                        char data_buf[32];
                        htoa(data, data_buf);
                        kprint(data_buf);
                    } else {
                        uint32_t data = va_arg(format_list, uint32_t);
                        char data_buf[32];
                        htoa((uint64_t) data, data_buf);
                        kprint(data_buf);
                    }
                    break;
                case 'd':
                    if (big) {
                        int64_t data = va_arg(format_list, int64_t);
                        char data_buf[32];
                        itoa(data, data_buf);
                        kprint(data_buf);
                    } else {
                        int32_t data = va_arg(format_list, int32_t);
                        char data_buf[32];
                        itoa((int64_t) data, data_buf);
                        kprint(data_buf);
                    }
                    break;
                case 'u':
                    if (big) {
                        uint64_t data = va_arg(format_list, uint64_t);
                        char data_buf[32];
                        utoa(data, data_buf);
                        kprint(data_buf);
                    } else {
                        uint32_t data = va_arg(format_list, uint32_t);
                        char data_buf[32];
                        utoa((uint32_t) data, data_buf);
                        kprint(data_buf);
                    }
                    break;
                case 's':
                    if (big) {
                        (void) va_arg(format_list, uint64_t);
                    } else {
                        kprint(va_arg(format_list, char *));
                    }
                    break;

                default:
                    break;
            }
        } else {
            tty_out(message[index], &base_tty);
        }
        index++;
    }

    va_end(format_list);
    flip_buffers();
    unlock(base_tty.tty_lock);
    //yield();
}

void kprintf_yieldless(char *message, ...) {
    lock(base_tty.tty_lock);
    va_list format_list;
    uint64_t index = 0;
    uint8_t big = 0;

    va_start(format_list, message);

    while (message[index]) {
        if (message[index] == '%') {
            index++;
            if (message[index] == 'l') {
                index++;
                big = 1;
            }
            switch (message[index]) {
                case 'x':
                    if (big) {
                        uint64_t data = va_arg(format_list, uint64_t);
                        char data_buf[32];
                        htoa(data, data_buf);
                        kprint(data_buf);
                    } else {
                        uint32_t data = va_arg(format_list, uint32_t);
                        char data_buf[32];
                        htoa((uint64_t) data, data_buf);
                        kprint(data_buf);
                    }
                    break;
                case 'd':
                    if (big) {
                        int64_t data = va_arg(format_list, int64_t);
                        char data_buf[32];
                        itoa(data, data_buf);
                        kprint(data_buf);
                    } else {
                        int32_t data = va_arg(format_list, int32_t);
                        char data_buf[32];
                        itoa((int64_t) data, data_buf);
                        kprint(data_buf);
                    }
                    break;
                case 'u':
                    if (big) {
                        uint64_t data = va_arg(format_list, uint64_t);
                        char data_buf[32];
                        utoa(data, data_buf);
                        kprint(data_buf);
                    } else {
                        uint32_t data = va_arg(format_list, uint32_t);
                        char data_buf[32];
                        utoa((uint32_t) data, data_buf);
                        kprint(data_buf);
                    }
                    break;
                case 's':
                    if (big) {
                        (void) va_arg(format_list, uint64_t);
                    } else {
                        kprint(va_arg(format_list, char *));
                    }
                    break;

                default:
                    break;
            }
        } else {
            tty_out(message[index], &base_tty);
        }
        index++;
    }

    va_end(format_list);
    flip_buffers();
    unlock(base_tty.tty_lock);
}

void safe_kprintf(char *message, ...) {
    lock(base_tty.tty_lock);
    va_list format_list;
    uint64_t index = 0;
    uint8_t big = 0;

    va_start(format_list, message);

    while (message[index]) {
        if (message[index] == '%') {
            index++;
            if (message[index] == 'l') {
                index++;
                big = 1;
            }
            switch (message[index]) {
                case 'x':
                    if (big) {
                        uint64_t data = va_arg(format_list, uint64_t);
                        char data_buf[32];
                        htoa(data, data_buf);
                        kprint(data_buf);
                    } else {
                        uint32_t data = va_arg(format_list, uint32_t);
                        char data_buf[32];
                        htoa((uint64_t) data, data_buf);
                        kprint(data_buf);
                    }
                    break;
                case 'd':
                    if (big) {
                        int64_t data = va_arg(format_list, int64_t);
                        char data_buf[32];
                        itoa(data, data_buf);
                        kprint(data_buf);
                    } else {
                        int32_t data = va_arg(format_list, int32_t);
                        char data_buf[32];
                        itoa((int64_t) data, data_buf);
                        kprint(data_buf);
                    }
                    break;
                case 'u':
                    if (big) {
                        uint64_t data = va_arg(format_list, uint64_t);
                        char data_buf[32];
                        utoa(data, data_buf);
                        kprint(data_buf);
                    } else {
                        uint32_t data = va_arg(format_list, uint32_t);
                        char data_buf[32];
                        utoa((uint32_t) data, data_buf);
                        kprint(data_buf);
                    }
                    break;
                case 's':
                    if (big) {
                        (void) va_arg(format_list, uint64_t);
                    } else {
                        kprint(va_arg(format_list, char *));
                    }
                    break;

                default:
                    break;
            }
        } else {
            tty_out(message[index], &base_tty);
        }
        index++;
    }

    va_end(format_list);
    flip_buffers();
    unlock(base_tty.tty_lock);
}