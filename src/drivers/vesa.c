#include "vesa.h"
#include "mm/vmm.h"
#include "klibc/stdlib.h"
#include "klibc/string.h"
#include "klibc/lock.h"
#include "drivers/serial.h"

vesa_info_t vesa_display_info;
lock_t vesa_lock = 0;

void init_vesa(multiboot_info_t *mb) {
    mb = (multiboot_info_t *) ((uint64_t) mb + 0xFFFF800000000000);
    /* Size info */
    vesa_display_info.width = mb->framebuffer_width;
    vesa_display_info.height = mb->framebuffer_height;
    vesa_display_info.pitch = mb->framebuffer_pitch;
    vesa_display_info.framebuffer_size = vesa_display_info.height * vesa_display_info.pitch;
    vesa_display_info.framebuffer_pixels = vesa_display_info.width * vesa_display_info.height;
    /* Bitshift sizes */
    vesa_display_info.red_shift = mb->framebuffer_red_field_position;
    vesa_display_info.green_shift = mb->framebuffer_green_field_position;
    vesa_display_info.blue_shift = mb->framebuffer_blue_field_position;
    /* Framebuffer address */
    vesa_display_info.framebuffer = (uint32_t *) kcalloc(vesa_display_info.framebuffer_size);
    vesa_display_info.actual_framebuffer = (uint32_t *) mb->framebuffer_addr;

    /* Map the real framebuffer and as write combining */
    vmm_map((void *) mb->framebuffer_addr, (void *) mb->framebuffer_addr, 
        (vesa_display_info.framebuffer_size + 0x1000 - 1) / 0x1000, 
        VMM_PRESENT | VMM_WRITE);
    vmm_set_pat((void *) mb->framebuffer_addr, (vesa_display_info.framebuffer_size + 0x1000 - 1) / 0x1000, 1);
    
    /* Map the double buffer and as write combining */
    void *phys_double_buffer = virt_to_phys(vesa_display_info.framebuffer, (pt_t *) vmm_get_pml4t());

    vmm_map(phys_double_buffer, phys_double_buffer, 
        (vesa_display_info.framebuffer_size + 0x1000 - 1) / 0x1000, 
        VMM_PRESENT | VMM_WRITE | VMM_USER);

    vesa_display_info.framebuffer = (uint32_t *) phys_double_buffer;

    sprintf("\n[VESA] Loaded a %lu x %lu display", vesa_display_info.width, vesa_display_info.height);
}

void put_pixel(uint64_t x, uint64_t y, color_t color) {
    lock(&vesa_lock);
    uint32_t color_dat = (color.r << vesa_display_info.red_shift) |
        (color.g << vesa_display_info.green_shift) |
        (color.b << vesa_display_info.blue_shift);
    
    uint64_t offset = (y * vesa_display_info.pitch) + (x * 4);
    *(uint32_t *) ((uint64_t) vesa_display_info.framebuffer + offset) = color_dat;
    unlock(&vesa_lock);
}

void render_font(uint8_t font[128][8], char c, uint64_t x, uint64_t y, color_t fg, color_t bg) {
    lock(&vesa_lock);
    for (uint8_t iy = 0; iy < 8; iy++) {
        for (uint8_t ix = 0; ix < 8; ix++) {
            if ((font[(uint8_t) c][iy] >> ix) & 1) {
                uint32_t color_dat = (fg.r << vesa_display_info.red_shift) |
                    (fg.g << vesa_display_info.green_shift) |
                    (fg.b << vesa_display_info.blue_shift);
                
                uint64_t offset = ((iy + y) * vesa_display_info.pitch) + ((ix + x) * 4);
                *(uint32_t *) ((uint64_t) vesa_display_info.framebuffer + offset) = color_dat;
            } else {
                uint32_t color_dat = (bg.r << vesa_display_info.red_shift) |
                    (bg.g << vesa_display_info.green_shift) |
                    (bg.b << vesa_display_info.blue_shift);
                
                uint64_t offset = ((iy + y) * vesa_display_info.pitch) + ((ix + x) * 4);
                *(uint32_t *) ((uint64_t) vesa_display_info.framebuffer + offset) = color_dat;
            }
        }
    }
    unlock(&vesa_lock);
}

void vesa_scroll(uint64_t rows_shift, color_t bg) {
    lock(&vesa_lock);

    uint32_t color_dat = (bg.r << vesa_display_info.red_shift) |
        (bg.g << vesa_display_info.green_shift) |
        (bg.b << vesa_display_info.blue_shift);

    uint64_t buf_pos = (uint64_t) vesa_display_info.framebuffer;
    /* Do the copy */
    memcpy32((uint32_t *) (buf_pos + (vesa_display_info.pitch * rows_shift)), (uint32_t *) (buf_pos), 
        vesa_display_info.framebuffer_pixels - ((vesa_display_info.pitch * rows_shift) / 4));
    /* Do the clear */
    memset32((uint32_t *) (buf_pos + vesa_display_info.framebuffer_size - (vesa_display_info.pitch * rows_shift)),
        color_dat, (vesa_display_info.pitch * rows_shift) / 4);
    unlock(&vesa_lock);
}

void flip_buffers() {
    lock(&vesa_lock);
    memcpy32(vesa_display_info.framebuffer, vesa_display_info.actual_framebuffer,
        vesa_display_info.framebuffer_pixels);
    unlock(&vesa_lock);
}

void clear_buffer() {
    lock(&vesa_lock);

    /* Set the buffer to 0 */
    memset32(vesa_display_info.framebuffer, 0, vesa_display_info.framebuffer_pixels);
    memcpy32(vesa_display_info.framebuffer, vesa_display_info.actual_framebuffer,
        vesa_display_info.framebuffer_pixels);

    unlock(&vesa_lock);
}

void fill_screen(color_t color) {
    lock(&vesa_lock);

    /* Create the color */
    uint32_t color_dat = (color.r << vesa_display_info.red_shift) |
                    (color.g << vesa_display_info.green_shift) |
                    (color.b << vesa_display_info.blue_shift);

    /* Fill and flip the buffers */
    memset32(vesa_display_info.framebuffer, color_dat, vesa_display_info.framebuffer_pixels);
    memcpy32(vesa_display_info.framebuffer, vesa_display_info.actual_framebuffer,
        vesa_display_info.framebuffer_pixels);
    unlock(&vesa_lock);
}