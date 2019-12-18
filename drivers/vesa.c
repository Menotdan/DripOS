#include "vesa.h"

color_t color_from_rgb(uint8_t r, uint8_t g, uint8_t b) {
    color_t ret;
    ret.red = r;
    ret.green = g;
    ret.blue = b;
    return ret;
}

void draw_pixel(uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b) {
    uint8_t *vidmemcur = vidmem;
    uint32_t offset = y*bpl + x*bbp;
    vidmemcur += offset;
    for (uint8_t byte = 0; byte<bbp; byte++) {
        if (red_byte == byte) {
            *vidmemcur = r;
        } else if (green_byte == byte) {
            *vidmemcur = g;
        } else if (blue_byte == byte) {
            *vidmemcur = b;
        }
        vidmemcur++;
    }
}

void fill_screen(uint8_t r, uint8_t g, uint8_t b) {
    uint8_t *vidmemcur = vidmem;
    uint32_t byte_counter = 0;
    for (uint32_t x = 0; x<width; x++) {
        for (uint32_t y = 0; y<height; y++) {
            for (uint8_t byte = 0; byte<bbp; byte++) {
                if (red_byte == byte) {
                    *vidmemcur = r;
                } else if (green_byte == byte) {
                    *vidmemcur = g;
                } else if (blue_byte == byte) {
                    *vidmemcur = b;
                }
                vidmemcur++;
                byte_counter++;
            }
            //vidmemcur += bpl-byte_counter;
            byte_counter = 0;
        }
    }
}

void render8x8bitmap(unsigned char bitmap[8], uint8_t xpos, uint8_t ypos, color_t bg, color_t fg) {
    char current_map;
    for (uint8_t y = 0; y<8; y++) {
        current_map = bitmap[y];
        for (uint8_t x = 0; x<8; x++) {
            uint8_t active = (current_map >> x) & 1; // Get the bit
            if (active == 1) {
                draw_pixel(x+(xpos*8),y+(ypos*8),fg.red, fg.green, fg.blue);
            } else {
                draw_pixel(x+(xpos*8),y+(ypos*8),bg.red,bg.green,bg.blue);
            }
        }
    }
}