#include "vesa.h"
#include "mm/vmm.h"
#include "klibc/stdlib.h"
#include "klibc/string.h"
#include "klibc/lock.h"
#include "drivers/serial.h"
#include "proc/ipc.h" // IPC server for VESA

/* Setup vfs driver */
#include "fs/vfs/vfs.h"
#include "fs/devfs/devfs.h"
#include "fs/fd.h"
#include "proc/scheduler.h"
#include "klibc/errno.h"

vesa_info_t vesa_display_info;
lock_t vesa_lock = {0, 0, 0, 0};

void init_vesa(stivale_info_t *bootloader_info) {
    bootloader_info = GET_HIGHER_HALF(stivale_info_t *, bootloader_info);

    /* Size info */
    vesa_display_info.width = bootloader_info->framebuffer_width;
    vesa_display_info.height = bootloader_info->framebuffer_height;
    vesa_display_info.pitch = bootloader_info->framebuffer_pitch;
    vesa_display_info.framebuffer_size = vesa_display_info.height * vesa_display_info.pitch;
    vesa_display_info.framebuffer_pixels = vesa_display_info.width * vesa_display_info.height;
    /* Bitshift sizes */
    vesa_display_info.red_shift   = 16;
    vesa_display_info.green_shift = 8;
    vesa_display_info.blue_shift  = 0;
    /* Framebuffer address */
    vesa_display_info.framebuffer = (uint32_t *) kcalloc(vesa_display_info.framebuffer_size);
    vesa_display_info.actual_framebuffer = (uint32_t *) (bootloader_info->framebuffer_addr + NORMAL_VMA_OFFSET);

    /* Map the real framebuffer and as write combining */
    vmm_map((void *) bootloader_info->framebuffer_addr, (void *) (bootloader_info->framebuffer_addr + NORMAL_VMA_OFFSET), 
        (vesa_display_info.framebuffer_size + 0x1000 - 1) / 0x1000, 
        VMM_PRESENT | VMM_WRITE);
    vmm_set_pat((void *) (bootloader_info->framebuffer_addr + NORMAL_VMA_OFFSET), 
        (vesa_display_info.framebuffer_size + 0x1000 - 1) / 0x1000, 1);

    sprintf("[VESA] Loaded a %lu x %lu display\n", vesa_display_info.width, vesa_display_info.height);
    sprintf("[VESA] Framebuffer addresses: %lx, %lx, %lx\n", vesa_display_info.framebuffer, vesa_display_info.actual_framebuffer, bootloader_info->framebuffer_addr);
    sprintf("[VESA] Display info:\n");
    sprintf("  pitch: %u\n", bootloader_info->framebuffer_pitch);
    sprintf("  width: %u\n", bootloader_info->framebuffer_width);
    sprintf("  height: %u\n", bootloader_info->framebuffer_height);
    sprintf("  bpp: %u\n", bootloader_info->framebuffer_bpp);
}

void put_pixel(uint64_t x, uint64_t y, color_t color) {
    uint32_t color_dat = (color.r << vesa_display_info.red_shift) |
        (color.g << vesa_display_info.green_shift) |
        (color.b << vesa_display_info.blue_shift);
    
    uint64_t offset = (y * vesa_display_info.pitch) + (x * 4);

    lock(vesa_lock);
    *(volatile uint32_t *) ((uint64_t) vesa_display_info.framebuffer + offset) = color_dat;
    unlock(vesa_lock);
}

void render_font(uint8_t font[128][8], char c, uint64_t x, uint64_t y, color_t fg, color_t bg) {
    uint32_t color_dat_fg = (fg.r << vesa_display_info.red_shift) |
                    (fg.g << vesa_display_info.green_shift) |
                    (fg.b << vesa_display_info.blue_shift);
    uint32_t color_dat_bg = (bg.r << vesa_display_info.red_shift) |
                    (bg.g << vesa_display_info.green_shift) |
                    (bg.b << vesa_display_info.blue_shift);

    lock(vesa_lock);
    for (uint8_t iy = 0; iy < 8; iy++) {
        for (uint8_t ix = 0; ix < 8; ix++) {
            if ((font[(uint8_t) c][iy] >> ix) & 1) {
                uint64_t offset = ((iy + y) * vesa_display_info.pitch) + ((ix + x) * 4);
                *(uint32_t *) ((uint64_t) vesa_display_info.framebuffer + offset) = color_dat_fg;
            } else {                
                uint64_t offset = ((iy + y) * vesa_display_info.pitch) + ((ix + x) * 4);
                *(uint32_t *) ((uint64_t) vesa_display_info.framebuffer + offset) = color_dat_bg;
            }
        }
    }
    unlock(vesa_lock);
}

void vesa_scroll(uint64_t rows_shift, color_t bg) {
    lock(vesa_lock);

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
    unlock(vesa_lock);
}

void flip_buffers() {
    lock(vesa_lock);
    memcpy32(vesa_display_info.framebuffer, vesa_display_info.actual_framebuffer,
        vesa_display_info.framebuffer_pixels);
    unlock(vesa_lock);
}

void clear_buffer() {
    lock(vesa_lock);

    /* Set the buffer to 0 */
    memset32(vesa_display_info.framebuffer, 0, vesa_display_info.framebuffer_pixels);
    memcpy32(vesa_display_info.framebuffer, vesa_display_info.actual_framebuffer,
        vesa_display_info.framebuffer_pixels);

    unlock(vesa_lock);
}

void fill_screen(color_t color) {
    lock(vesa_lock);

    /* Create the color */
    uint32_t color_dat = (color.r << vesa_display_info.red_shift) |
                    (color.g << vesa_display_info.green_shift) |
                    (color.b << vesa_display_info.blue_shift);

    /* Fill and flip the buffers */
    memset32(vesa_display_info.framebuffer, color_dat, vesa_display_info.framebuffer_pixels);
    memcpy32(vesa_display_info.framebuffer, vesa_display_info.actual_framebuffer,
        vesa_display_info.framebuffer_pixels);
    unlock(vesa_lock);
}

uint64_t vesa_seek(int fd, uint64_t offset, int whence) {
    (void) offset;

    fd_entry_t *fd_data = fd_lookup(fd);
    if (whence == SEEK_END) {
        fd_data->seek = vesa_display_info.framebuffer_size;
    }
    return fd_data->seek;
}

int vesa_write(int fd, void *buf, uint64_t count) {
    fd_entry_t *fd_data = fd_lookup(fd);

    /* Align args to 4 bytes */
    if (count & 0b11) {
        return -EINVAL;
    }
    if (fd_data->seek & 0b11 || fd_data->seek + count > vesa_display_info.framebuffer_size) {
        return -ESPIPE;
    }

    lock(vesa_lock);
    uint32_t *fb_seeked = (uint32_t *)
        ((uint64_t) vesa_display_info.framebuffer + fd_data->seek);

    uint32_t *realfb_seeked = (uint32_t *)
        ((uint64_t) vesa_display_info.actual_framebuffer + fd_data->seek);

    memcpy32(buf, fb_seeked, count / 4);
    memcpy32(buf, realfb_seeked, count / 4);
    unlock(vesa_lock);
    fd_data->seek += count;

    return count;
}

int vesa_read(int fd, void *buf, uint64_t count) {
    fd_entry_t *fd_data = fd_lookup(fd);

    /* Align args to 4 bytes */
    if (count & 0b11) {
        return -EINVAL;
    }
    if (fd_data->seek & 0b11 || fd_data->seek + count > vesa_display_info.framebuffer_size) {
        return -ESPIPE;
    }

    lock(vesa_lock);
    uint32_t *fb_seeked = (uint32_t *)
        ((uint64_t) vesa_display_info.framebuffer + fd_data->seek);

    memcpy32(fb_seeked, buf, count / 4);
    unlock(vesa_lock);
    fd_data->seek += count;

    return count;
}

void setup_vesa_device() {
    vfs_ops_t ops = {devfs_open, 0, devfs_close, vesa_read, vesa_write, vesa_seek};
    register_device("vesafb", ops, (void *) 0);
}

void vesa_ipc_server() {
    int err = register_ipc_handle(1);
    if (err) {
        sprintf("[VESA] Failed to register IPC server on port 1!\n");
    }

    while (1) {
        ipc_handle_t *handle = wait_ipc(1);
        sprintf("[VESA] Got IPC request from pid %d\n", handle->pid);
        if (handle->operation_type == IPC_OPERATION_READ) {
            if ((long unsigned int) handle->size < sizeof(vesa_ipc_data_area_t)) {
                handle->err = IPC_BUFFER_TOO_SMALL;
                handle->size = sizeof(vesa_ipc_data_area_t);
                trigger_event(handle->ipc_completed);
                continue; // wait for a new event
            }

            interrupt_safe_lock(sched_lock);
            process_t *target_process = processes[handle->pid];
            interrupt_safe_unlock(sched_lock);

            /* Map framebuffer into the process */
            lock(target_process->brk_lock);
            void *map_framebuffer_addr = (void *) target_process->current_brk;
            target_process->current_brk += ((vesa_display_info.framebuffer_size + 0x1000 - 1) / 0x1000) * 0x1000;
            unlock(target_process->brk_lock);

            vmm_map_pages(GET_LOWER_HALF(void *, vesa_display_info.actual_framebuffer), map_framebuffer_addr, 
                (void *) target_process->cr3,
                (vesa_display_info.framebuffer_size + 0x1000 - 1) / 0x1000, 
                 VMM_PRESENT | VMM_WRITE | VMM_USER);
            vmm_set_pat_pages(map_framebuffer_addr, (void *) target_process->cr3,
                (vesa_display_info.framebuffer_size + 0x1000 - 1) / 0x1000, 1);

            vesa_ipc_data_area_t info = {map_framebuffer_addr, vesa_display_info.width, vesa_display_info.height, vesa_display_info.pitch};
            memcpy((uint8_t *) &info, handle->buffer, sizeof(vesa_ipc_data_area_t));

        } else {
            handle->err = IPC_OPERATION_NOT_SUPPORTED;
            trigger_event(handle->ipc_completed);
            continue; // wait for a new event
        }
        trigger_event(handle->ipc_completed);
    }
}