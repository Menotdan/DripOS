#ifndef VESA_H
#define VESA_H
#include <stdint.h>
#include "multiboot.h"

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} color_t;

typedef struct {
    uint64_t framebuffer_size;
    uint64_t framebuffer_pixels;
    uint64_t width;
    uint64_t height;
    uint64_t pitch;
    uint64_t red_shift;
    uint64_t green_shift;
    uint64_t blue_shift;

    uint32_t *framebuffer;
} vesa_info_t;

extern vesa_info_t vesa_display_info;

void init_vesa(multiboot_info_t *mb);
void put_pixel(uint64_t x, uint64_t y, color_t color);
void render_font(uint8_t font[128][8], char c, uint64_t x, uint64_t y, color_t fg, color_t bg);

#endif