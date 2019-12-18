#ifndef VESA_H
#define VESA_H
#include "../kernel/kernel.h"
#include "font.h"
#include <stdint.h>
#include "../libc/mem.h"

typedef struct color
{
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} color_t;

typedef struct vesa_tty
{
    uint8_t *text_vid_buffer;
    uint8_t *graphics_vid_buffer;
    uint32_t video_buffer_size;
    uint8_t text_enabled;
} vesa_tty_t;

void update_display();
void draw_pixel(uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b);
void fill_screen(uint8_t r, uint8_t g, uint8_t b);
extern void render8x8bitmap(unsigned char bitmap[8], uint8_t xpos, uint8_t ypos, color_t bg, color_t fg);
color_t color_from_rgb(uint8_t r, uint8_t g, uint8_t b);
vesa_tty_t current_screen;

#endif