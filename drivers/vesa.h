#ifndef VESA_H
#define VESA_H
#include "../kernel/kernel.h"
#include "font.h"
#include <stdint.h>
#include "../libc/mem.h"
#include "../drivers/serial.h"
#include "../libc/string.h"

typedef struct color
{
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} color_t;

typedef struct vesa_tty
{
    uint8_t *graphics_vid_buffer;
    uint32_t video_buffer_size;
    uint32_t x;
    uint32_t y;
    uint32_t buffer_width;
    uint32_t buffer_height;
    uint32_t text_col;
    uint32_t text_row;
} vesa_buffer_t;

void update_display();
void draw_pixel(uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b);
void fill_screen(uint8_t r, uint8_t g, uint8_t b);
void cleanup_framebuffer(vesa_buffer_t framebuffer);
extern void render8x8bitmap(unsigned char bitmap[8], uint8_t xpos, uint8_t ypos, color_t bg, color_t fg);
color_t color_from_rgb(uint8_t r, uint8_t g, uint8_t b);
color_t get_pixel(uint16_t x, uint16_t y);
vesa_buffer_t swap_display(vesa_buffer_t new);
vesa_buffer_t new_framebuffer(uint32_t x, uint32_t y, uint32_t w, uint32_t h);

vesa_buffer_t current_buffer;

#endif