#ifndef TTY_H
#define TTY_H
#include <stdint.h>
#include "drivers/vesa.h"

typedef struct {
    uint64_t c_pos_x;
    uint64_t c_pos_y;
    uint64_t cols;
    uint64_t rows;
    uint8_t *font;
    color_t fg;
    color_t bg;
} tty_t;

extern tty_t base_tty;

void tty_out(char c, tty_t *tty);
void tty_seek(uint64_t x, uint64_t y);
void tty_init(tty_t *tty, uint64_t font_width, uint64_t font_height);
void kprint(char *s);
void kprintf(char *message, ...);

#endif