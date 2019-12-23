/*
    vesa.c
    Copyright Menotdan 2018-2019

    VESA screen control

    MIT License
*/

#include <drivers/vesa.h>

vesa_tty_t current_screen;

void vesa_init() {

}

/* Create a new framebuffer with an x and y position on the screen,
and with a set width and height */
vesa_tty_t new_framebuffer(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    vesa_tty_t ret;
    ret.video_buffer_size = (w*h)*bbp;
    ret.graphics_vid_buffer = kmalloc(ret.video_buffer_size);
    ret.x = x;
    ret.y = y;
    ret.buffer_height = h;
    ret.buffer_width = w;
    ret.text_col = w/8;
    ret.text_row = h/8;
    return ret;
}

void cleanup_framebuffer(vesa_tty_t framebuffer) {
    free(framebuffer.graphics_vid_buffer, framebuffer.video_buffer_size);
}

color_t color_from_rgb(uint8_t r, uint8_t g, uint8_t b) {
    color_t ret;
    ret.red = r;
    ret.green = g;
    ret.blue = b;
    return ret;
}

void draw_pixel(uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b) {
    uint32_t *vidmemcur = (uint32_t *)current_screen.graphics_vid_buffer;
    uint32_t offset = ((y * current_screen.buffer_width) + x);
    vidmemcur += offset;
    *vidmemcur = (r << (red_byte)) | (g << (green_byte)) | (b << (blue_byte));
    vidmemcur++;
}

void fill_screen(uint8_t r, uint8_t g, uint8_t b) {
    uint32_t color = (r << (red_byte)) | (g << (green_byte)) | (b << (blue_byte));
    uint32_t *vidmemcur = (uint32_t *)(current_screen.graphics_vid_buffer);
    for (uint32_t i = 0; i<(current_screen.buffer_height*current_screen.buffer_width); i++) {
        *(uint32_t *)vidmemcur = color;
        vidmemcur++;
    }
}

void render8x8bitmap(unsigned char bitmap[8], uint8_t xpos, uint8_t ypos, color_t bg, color_t fg) {
    char current_map;
    uint32_t foreground_color = (fg.red << (red_byte)) | (fg.green << (green_byte)) | (fg.blue << (blue_byte));
    uint32_t background_color = (bg.red << (red_byte)) | (bg.green << (green_byte)) | (bg.blue << (blue_byte));;
    uint32_t offset = ((ypos*8)*current_screen.buffer_width) + (xpos*8);
    uint32_t *vidmemcur = (uint32_t *)current_screen.graphics_vid_buffer;
    vidmemcur += offset;
    for (uint8_t y = 0; y<8; y++) {
        current_map = bitmap[y];
        for (uint8_t x = 0; x<8; x++) {
            uint8_t active = (current_map >> x) & 1; // Get the bit
            if (active == 1) {
                /* Draw foreground color */
                *vidmemcur = foreground_color;
            } else {
                /* Draw background color */
                *vidmemcur = background_color;
            }
            vidmemcur++;
        }
        vidmemcur += current_screen.buffer_width-8;
    }
}

vesa_tty_t swap_display(vesa_tty_t new) {
    vesa_tty_t ret = current_screen;
    current_screen = new;
    return ret;
}

void update_display() {
    uint32_t *vidmemcur = (uint32_t *)(((uint32_t*)vidmem) + (current_screen.x +
    (current_screen.y * width))); // Video memory offset to draw the buffer at
    uint32_t *buffer_mem = (uint32_t *)current_screen.graphics_vid_buffer; // The buffer's memory to read from

    for (uint32_t i = 0; i<current_screen.buffer_height; i++) { // Iterate over the whole buffer
        memcpy32(buffer_mem, vidmemcur, current_screen.buffer_width); // Copy the current line to video memory
        buffer_mem += current_screen.buffer_width; // Increment the buffer memory
        vidmemcur += (width); // Increment the video memory
        // by the width of the video memory - the buffer's width to reset the drawing
        // to where the start of the buffer's space is 
    }
}