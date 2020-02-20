#include "tty.h"
#include "klibc/font.h"
#include "klibc/string.h"
#include "klibc/lock.h"
#include <stdarg.h>

#include "drivers/serial.h"

color_t default_fg = {255, 255, 255};
color_t default_bg = {0, 0, 0};
tty_t base_tty;

void tty_init(tty_t *tty, uint64_t font_width, uint64_t font_height) {
    (void) font_width;
    (void) font_height;
    tty->fg = default_fg;
    tty->bg = default_bg;
    tty->c_pos_x = 0;
    tty->c_pos_y = 0;
    tty->rows = vesa_display_info.height / font_height;
    tty->cols = vesa_display_info.width / font_width;
    tty->font = (uint8_t *) font8x8_basic;
    tty->tty_lock = 0;
}

void tty_out(char c, tty_t *tty) {
    if (c == '\n') {
        tty->c_pos_y += 1;
        tty->c_pos_x = 0;
    } else {
        render_font((uint8_t (*) [8]) tty->font, c, tty->c_pos_x * 8, tty->c_pos_y * 8, tty->fg, tty->bg);
        //char *buffer = (char *) 0xb8000;
        //buffer[((tty->c_pos_y * tty->cols) + tty->c_pos_x) * 2] = c;
        tty->c_pos_x++;
        if (tty->c_pos_x == tty->cols) {
            tty->c_pos_y += 1;
            tty->c_pos_x = 0;
        }
    }

    if (tty->c_pos_y == tty->rows) {
        vesa_scroll(8);
        tty->c_pos_y--;
    }
}

void tty_seek(uint64_t x, uint64_t y) {
    lock(&base_tty.tty_lock);
    if (x < base_tty.cols && y < base_tty.rows) {
        base_tty.c_pos_x = x;
        base_tty.c_pos_y = y;
    }
    unlock(&base_tty.tty_lock);
}

void kprint(char *s) {
    while (*s != '\0') {
        tty_out(*s++, &base_tty);
    }
}

void kprintf(char *message, ...) {
    lock(&base_tty.tty_lock);
    va_list format_list;
    uint64_t index = 0;
    uint8_t big = 0;

    sprintf("\nprint call");

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
                    kprint(va_arg(format_list, char *));
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
    unlock(&base_tty.tty_lock);
}