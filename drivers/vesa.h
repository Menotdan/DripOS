#ifndef VESA_H
#define VESA_H
#include "../drivers/serial.h"
#include "../kernel/kernel.h"
#include "../libc/mem.h"
#include "font.h"
#include <stdint.h>
#include <string.h>

typedef struct color {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} color_t;

typedef struct vesa_tty {
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
void fill_screen(uint8_t r, uint8_t g, uint8_t b);
void cleanup_framebuffer(vesa_buffer_t framebuffer);
void rect_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, color_t color);
extern void render8x8bitmap(
    unsigned char bitmap[8], uint8_t xpos, uint8_t ypos, color_t bg, color_t fg);
color_t color_from_rgb(uint8_t r, uint8_t g, uint8_t b);
color_t get_pixel(uint16_t x, uint16_t y);
vesa_buffer_t swap_display(vesa_buffer_t new_buffer);
vesa_buffer_t new_framebuffer(uint32_t x, uint32_t y, uint32_t w, uint32_t h);

vesa_buffer_t current_buffer;

static inline void draw_pixel(uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b) {
    uint32_t *vidmemcur = (uint32_t *)current_buffer.graphics_vid_buffer;
    uint32_t offset = ((y * current_buffer.buffer_width) + x);
    vidmemcur += offset;
    *vidmemcur = (r << (red_byte)) | (g << (green_byte)) | (b << (blue_byte));
}

#endif