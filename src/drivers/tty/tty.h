#ifndef TTY_H
#define TTY_H
#include <stdint.h>
#include "fs/vfs/vfs.h"
#include "drivers/vesa.h"
#include "klibc/lock.h"

typedef struct {
    uint64_t c_pos_x;
    uint64_t c_pos_y;
    uint64_t cols;
    uint64_t rows;
    uint8_t *font;
    color_t fg;
    color_t bg;
    lock_t tty_lock;

    char *kb_in_buffer;
    uint64_t kb_in_buffer_index;
} tty_t;

extern tty_t base_tty;

int tty_dev_write(fd_entry_t *fd_data, void *buf, uint64_t count);
int tty_dev_read(fd_entry_t *fd_data, void *buf, uint64_t count);

char tty_get_char(tty_t *tty);
void tty_in(char c, tty_t *tty);
void tty_out(char c, tty_t *tty);
void tty_seek(uint64_t x, uint64_t y, tty_t *tty);
void tty_clear(tty_t *tty);
void tty_init(tty_t *tty, uint64_t font_width, uint64_t font_height);
void kprint(char *s);
void kprintf(char *message, ...);

#endif