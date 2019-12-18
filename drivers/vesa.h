#ifndef VESA_H
#define VESA_H
#include "../kernel/kernel.h"
#include "font.h"
#include <stdint.h>

typedef struct color
{
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} color_t;

void draw_pixel(uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b);
void fill_screen(uint8_t r, uint8_t g, uint8_t b);
extern void render8x8bitmap(unsigned char bitmap[8], uint8_t xpos, uint8_t ypos, color_t bg, color_t fg);
color_t color_from_rgb(uint8_t r, uint8_t g, uint8_t b);

#endif