#ifndef VESA_H
#define VESA_H
#include "../kernel/kernel.h"
#include "font.h"
#include <stdint.h>

void draw_pixel(uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b);
void fill_screen(uint8_t r, uint8_t g, uint8_t b);
void render8x8bitmap(unsigned char bitmap[8]);
#endif