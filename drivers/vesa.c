#include "vesa.h"

vesa_buffer_t current_buffer;

void vesa_init() {

}

void rect_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, color_t color) {
    for (uint32_t y_draw = 0; y_draw < h; y_draw++) {
        for (uint32_t x_draw = 0; x_draw < w; x_draw++) {
            draw_pixel((x + x_draw), (y + y_draw), color.red, color.blue, color.green);
        }
    }
}

/* Create a new framebuffer with an x and y position on the screen,
and with a set width and height */
vesa_buffer_t new_framebuffer(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    vesa_buffer_t ret;
    ret.video_buffer_size = (w*h)*bbp;
    ret.graphics_vid_buffer = kmalloc(ret.video_buffer_size);
    memset32((uint32_t *)ret.graphics_vid_buffer, 0, ((ret.video_buffer_size)/4));
    ret.x = x;
    ret.y = y;
    ret.buffer_height = h;
    ret.buffer_width = w;
    ret.text_col = w/8;
    ret.text_row = h/8;
    return ret;
}

void cleanup_framebuffer(vesa_buffer_t framebuffer) {
    free(framebuffer.graphics_vid_buffer, framebuffer.video_buffer_size);
    update_display();
}

color_t color_from_rgb(uint8_t r, uint8_t g, uint8_t b) {
    color_t ret;
    ret.red = r;
    ret.green = g;
    ret.blue = b;
    return ret;
}

color_t get_pixel(uint16_t x, uint16_t y) {
    color_t ret;

    uint32_t *vidmemcur = (uint32_t *)current_buffer.graphics_vid_buffer;
    uint32_t offset = ((y * current_buffer.buffer_width) + x);
    vidmemcur += offset;
    ret.red = (uint8_t)(*vidmemcur >> (red_byte));
    ret.green = (uint8_t)(*vidmemcur >> (green_byte));
    ret.blue = (uint8_t)(*vidmemcur >> (blue_byte));
    return ret;
}

void fill_screen(uint8_t r, uint8_t g, uint8_t b) {
    uint32_t color = (r << (red_byte)) | (g << (green_byte)) | (b << (blue_byte));
    uint32_t *vidmemcur = (uint32_t *)(current_buffer.graphics_vid_buffer);
    for (uint32_t i = 0; i<(current_buffer.buffer_height*current_buffer.buffer_width); i++) {
        *(uint32_t *)vidmemcur = color;
        vidmemcur++;
    }
}

void render8x8bitmap(unsigned char bitmap[8], uint8_t xpos, uint8_t ypos, color_t bg, color_t fg) {
    char current_map;
    uint32_t foreground_color = (fg.red << (red_byte)) | (fg.green << (green_byte)) | (fg.blue << (blue_byte));
    uint32_t background_color = (bg.red << (red_byte)) | (bg.green << (green_byte)) | (bg.blue << (blue_byte));;
    uint32_t offset = ((ypos*8)*current_buffer.buffer_width) + (xpos*8);
    uint32_t *vidmemcur = (uint32_t *)current_buffer.graphics_vid_buffer;
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
        vidmemcur += current_buffer.buffer_width-8;
    }
}

vesa_buffer_t swap_display(vesa_buffer_t new) {
    vesa_buffer_t ret = current_buffer;
    current_buffer = new;
    return ret;
}

void update_display() {
    uint32_t *vidmemcur = (uint32_t *)(((uint32_t*)vidmem) + (current_buffer.x +
    (current_buffer.y * width))); // Video memory offset to draw the buffer at
    uint32_t *buffer_mem = (uint32_t *)current_buffer.graphics_vid_buffer; // The buffer's memory to read from

    for (uint32_t i = 0; i<current_buffer.buffer_height; i++) { // Iterate over the whole buffer
        memcpy32(buffer_mem, vidmemcur, current_buffer.buffer_width); // Copy the current line to video memory
        buffer_mem += current_buffer.buffer_width; // Increment the buffer memory
        vidmemcur += (width); // Increment the video memory
        // by the width of the video memory - the buffer's width to reset the drawing
        // to where the start of the buffer's space is 
    }
}