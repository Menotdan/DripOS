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
    // uint8_t *vidmemcur = vidmem;
    // uint32_t offset = y*bpl + x*bbp;
    // vidmemcur += offset;
    // for (uint8_t byte = 0; byte<bbp; byte++) {
    //     if (red_byte == byte) {
    //         *vidmemcur = r;
    //     } else if (green_byte == byte) {
    //         *vidmemcur = g;
    //     } else if (blue_byte == byte) {
    //         *vidmemcur = b;
    //     }
    //     vidmemcur++;
    // }

    uint8_t *vidmemcur = current_screen.graphics_vid_buffer;
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
    // uint8_t *vidmemcur = vidmem;
    // uint32_t byte_counter = 0;
    // for (uint32_t y = 0; y<height; y++) {
    //     for (uint32_t x = 0; x<width; x++) {
    //         for (uint8_t byte = 0; byte<bbp; byte++) {
    //             if (red_byte == byte) {
    //                 *vidmemcur = r;
    //             } else if (green_byte == byte) {
    //                 *vidmemcur = g;
    //             } else if (blue_byte == byte) {
    //                 *vidmemcur = b;
    //             }
    //             vidmemcur++;
    //             byte_counter++;
    //         }

    //         byte_counter = 0;
    //     }
    // }

    uint8_t *vidmemcur = current_screen.graphics_vid_buffer;
    for (uint32_t y = 0; y<height; y++) {
        for (uint32_t x = 0; x<width; x++) {
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
    }

    vidmemcur = current_screen.text_vid_buffer;
    for (uint32_t y = 0; y<height; y++) {
        for (uint32_t x = 0; x<width; x++) {
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
    }
}

void render8x8bitmap(unsigned char bitmap[8], uint8_t xpos, uint8_t ypos, color_t bg, color_t fg) {
    char current_map;
    for (uint8_t y = 0; y<8; y++) {
        current_map = bitmap[y];
        for (uint8_t x = 0; x<8; x++) {
            uint8_t active = (current_map >> x) & 1; // Get the bit
            if (active == 1) {
                /* Draw foreground color */
                // uint8_t *vidmemcur = vidmem;
                // uint32_t offset = (y+(ypos*8))*bpl + (x+(xpos*8))*bbp;
                // vidmemcur += offset;
                // for (uint8_t byte = 0; byte<bbp; byte++) {
                //     if (red_byte == byte) {
                //         *vidmemcur = fg.red;
                //     } else if (green_byte == byte) {
                //         *vidmemcur = fg.green;
                //     } else if (blue_byte == byte) {
                //         *vidmemcur = fg.blue;
                //     }
                //     vidmemcur++;
                // }

                uint8_t *vidmemcur = current_screen.text_vid_buffer;
                uint32_t offset = (y+(ypos*8))*bpl + (x+(xpos*8))*bbp;
                vidmemcur += offset;
                for (uint8_t byte = 0; byte<bbp; byte++) {
                    if (red_byte == byte) {
                        *vidmemcur = fg.red;
                    } else if (green_byte == byte) {
                        *vidmemcur = fg.green;
                    } else if (blue_byte == byte) {
                        *vidmemcur = fg.blue;
                    }
                    vidmemcur++;
                }
            } else {
                /* Draw background color */
                uint8_t *vidmemcur = current_screen.text_vid_buffer;
                uint32_t offset = (y+(ypos*8))*bpl + (x+(xpos*8))*bbp;
                vidmemcur += offset;
                for (uint8_t byte = 0; byte<bbp; byte++) {
                    if (red_byte == byte) {
                        *vidmemcur = bg.red;
                    } else if (green_byte == byte) {
                        *vidmemcur = bg.green;
                    } else if (blue_byte == byte) {
                        *vidmemcur = bg.blue;
                    }
                    vidmemcur++;
                }
            }
        }
    }
}

void update_display() {
    uint8_t *vidmemcur = vidmem;
    uint8_t *text = current_screen.text_vid_buffer;
    uint8_t pixel_buffer[bbp];
    memory_copy(current_screen.graphics_vid_buffer, vidmem, current_screen.video_buffer_size);
    if (vidmemcur) {

    } 

    if (current_screen.text_enabled) {
        uint8_t *vidmemcur = vidmem;
        for (uint32_t buffer_pos = 0; buffer_pos<current_screen.video_buffer_size/bbp; buffer_pos++) {
            uint8_t all_zero = 1;
            for (uint8_t byte = 0; byte<bbp; byte++) {
                pixel_buffer[byte] = *text;
                text++;
                if (pixel_buffer[byte] != 0) {
                    all_zero = 0;
                }
            }
            if (all_zero == 0) {
                memory_copy(pixel_buffer, vidmemcur, bbp);
            }
            vidmemcur += bbp;
        }
    }
}