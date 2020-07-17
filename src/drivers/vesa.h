#ifndef VESA_H
#define VESA_H
#include <stdint.h>
#include "stivale.h"
#include "klibc/lock.h"

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
    uint32_t *actual_framebuffer;
} vesa_info_t;

typedef struct {
    uint32_t *buffer_pointer;
    uint64_t width;
    uint64_t height;
    uint64_t pitch;
} __attribute__((packed)) vesa_ipc_data_area_t;


void init_vesa(stivale_info_t *bootloader_info);
void put_pixel(uint64_t x, uint64_t y, color_t color);
void render_font(uint8_t font[128][8], char c, uint64_t x, uint64_t y, color_t fg, color_t bg);
void vesa_scroll(uint64_t rows_shift, color_t bg);
void flip_buffers();
void clear_buffer();
void fill_screen(color_t color);

void setup_vesa_device();

void vesa_ipc_server();

extern lock_t vesa_lock;
extern vesa_info_t vesa_display_info;

#endif