#include "vesa.h"

vesa_tty_t current_screen;

color_t color_from_rgb(uint8_t r, uint8_t g, uint8_t b) {
    color_t ret;
    ret.red = r;
    ret.green = g;
    ret.blue = b;
    return ret;
}

void draw_pixel(uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b) {
    uint32_t *vidmemcur = (uint32_t *)current_screen.graphics_vid_buffer;
    uint32_t offset = (y*bpl + x*bbp)/4;
    vidmemcur += offset;
    *vidmemcur = (r << (red_byte)) | (g << (green_byte)) | (b << (blue_byte));
    vidmemcur++;
}

void fill_screen(uint8_t r, uint8_t g, uint8_t b) {
    uint32_t color = (r << (red_byte)) | (g << (green_byte)) | (b << (blue_byte));
    uint32_t *vidmemcur = (uint32_t *)(current_screen.graphics_vid_buffer);
    for (uint32_t i = 0; i<(height*bpl)/4; i++) {
        *(uint32_t *)vidmemcur = color;
        vidmemcur++;
    }
}

void render8x8bitmap(unsigned char bitmap[8], uint8_t xpos, uint8_t ypos, color_t bg, color_t fg) {
    char current_map;
    uint32_t foreground_color = (fg.red << (red_byte)) | (fg.green << (green_byte)) | (fg.blue << (blue_byte));
    uint32_t background_color = (bg.red << (red_byte)) | (bg.green << (green_byte)) | (bg.blue << (blue_byte));;
    for (uint8_t y = 0; y<8; y++) {
        current_map = bitmap[y];
        for (uint8_t x = 0; x<8; x++) {
            uint8_t active = (current_map >> x) & 1; // Get the bit
            if (active == 1) {
                /* Draw foreground color */

                uint32_t *vidmemcur = (uint32_t *)current_screen.graphics_vid_buffer;
                uint32_t offset = ((y+(ypos*8))*bpl + (x+(xpos*8))*bbp)/4;
                vidmemcur += offset;
                *vidmemcur = foreground_color;
                vidmemcur++;
            } else {
                /* Draw background color */

                uint32_t *vidmemcur = (uint32_t *)current_screen.graphics_vid_buffer;
                uint32_t offset = ((y+(ypos*8))*bpl + (x+(xpos*8))*bbp)/4;
                vidmemcur += offset;
                *vidmemcur = background_color;
                vidmemcur++;
            }
        }
    }
}

void update_display() {
    memcpy32((uint32_t *)current_screen.graphics_vid_buffer, (uint32_t*)vidmem, (current_screen.video_buffer_size/4));
}