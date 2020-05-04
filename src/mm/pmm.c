#include "pmm.h"
#include "mm/vmm.h"
#include "klibc/math.h"
#include "klibc/string.h"
#include "klibc/lock.h"
#include "drivers/serial.h"

uint8_t *bitmap;
uint64_t bitmap_max_page;

uint64_t total_memory = 0;
uint64_t used_memory = 0;
uint64_t available_memory = 0;

lock_t pmm_lock = {0, 0, 0, 0};

static void pmm_set_bit(uint64_t page) {
    uint8_t bit = page % 8;
    uint64_t byte = page / 8;

    bitmap[byte] |= (1<<bit);
}

static void pmm_clear_bit(uint64_t page) {
    uint8_t bit = page % 8;
    uint64_t byte = page / 8;

    bitmap[byte] &= ~(1<<bit);
}

static uint8_t pmm_get_bit(uint64_t page) {
    uint8_t bit = page % 8;
    uint64_t byte = page / 8;

    return bitmap[byte] & (1<<bit);
}

void pmm_memory_setup(stivale_info_t *bootloader_info) {
    // Bitmap set to kernel_end rounded up to a page
    bootloader_info = GET_HIGHER_HALF(stivale_info_t *, bootloader_info);
    sprintf("\n%lx bootloader info addr", bootloader_info);
    bitmap = (uint8_t *) ((((uint64_t) __kernel_end) + 0x1000 - 1) & ~(0xfff));

    // Dont destroy the bootloader info
    if ((uint64_t) bootloader_info + sizeof(stivale_info_t) > (uint64_t) bitmap) {
        bitmap = (uint8_t *) ((uint64_t) bootloader_info + sizeof(stivale_info_t));
    }

    // Setup the bitmap for the PMM
    e820_entry_t *mmap = GET_HIGHER_HALF(e820_entry_t *, bootloader_info->memory_map_addr);
    sprintf("\ne820 addrs: %lx %lx", bootloader_info->memory_map_addr, mmap);
    sprintf("\n %u x %u", bootloader_info->framebuffer_width, bootloader_info->framebuffer_height);

    // Calculate length of memory
    e820_entry_t start = mmap[0];
    e820_entry_t end = mmap[bootloader_info->memory_map_entries - 1];
    uint64_t mem_length = ROUND_UP((end.addr + end.len) - start.addr, 0x1000);
    uint64_t bitmap_bytes = ((mem_length / 0x1000) + 8 - 1) / 8;

    // Set bitmap as ones
    memset(bitmap, 0xFF, bitmap_bytes);

    for (uint64_t i = 0; i < bootloader_info->memory_map_entries; i++) {
        uint64_t rounded_block_start = ROUND_UP(mmap[i].addr, 0x1000);
        uint64_t rounded_block_end = ROUND_UP((mmap[i].addr + mmap[i].len), 0x1000);
        uint64_t rounded_block_len = rounded_block_end - rounded_block_start;

        sprintf("\n%lx - %lx", mmap[i].addr, mmap[i].addr + mmap[i].len);
        if (mmap[i].type == STIVALE_MEMORY_AVAILABLE && mmap[i].addr >= 0x100000) { // Ignore low 640K so we dont use it
            for (uint64_t i = 0; i < rounded_block_len / 0x1000; i++) {
                pmm_clear_bit(i + (rounded_block_start / 0x1000)); // set the memory as avaiable
            }
            available_memory += rounded_block_len;
        }
    }
    total_memory = available_memory;

    // Make sure the kernel and bitmap are not marked as available
    uint64_t kernel_start = (uint64_t) __kernel_start;
    sprintf("\nkernel_start: %lx %lx", kernel_start, kernel_start - KERNEL_VMA_OFFSET);
    uint64_t kernel_bitmap_len = ((uint64_t) bitmap + bitmap_bytes) - kernel_start;
    uint64_t kernel_bitmap_start_page = (kernel_start - KERNEL_VMA_OFFSET) / 0x1000;
    uint64_t kernel_bitmap_pages = (kernel_bitmap_len + 0x1000 - 1) / 0x1000;

    for (uint64_t i = 0; i < kernel_bitmap_pages; i++) {
        pmm_set_bit(i + kernel_bitmap_start_page);
    }
    bitmap_max_page = bitmap_bytes * 8;

    // Get rid of qloader2's CR3
    void *new_cr3 = pmm_alloc(0x1000);
    memset(GET_HIGHER_HALF(uint8_t *, new_cr3), 0, 0x1000);

    for (uint64_t i = 0; i < bootloader_info->memory_map_entries; i++) {
        uint64_t rounded_block_start = ROUND_UP(mmap[i].addr, 0x1000);
        uint64_t rounded_block_end = ROUND_UP((mmap[i].addr + mmap[i].len), 0x1000);
        uint64_t rounded_block_len = rounded_block_end - rounded_block_start;

        vmm_map_pages((void *) rounded_block_start, GET_HIGHER_HALF(void *, rounded_block_start), new_cr3, 
            (rounded_block_len + 0x1000 - 1) / 0x1000,
            VMM_PRESENT | VMM_WRITE);
    }

    pt_t *cr3 = GET_HIGHER_HALF(pt_t *, new_cr3);
    pt_t *cur = GET_HIGHER_HALF(pt_t *, vmm_get_pml4t());
    cr3->table[511] = cur->table[511];

    vmm_set_pml4t((uint64_t) new_cr3);

    base_kernel_cr3 = vmm_get_pml4t();
}

static uint64_t find_free_page(uint64_t pages) {
    uint64_t found_pages = 0;
    uint64_t first_page = 0;

    for (uint64_t i = 0; i < bitmap_max_page; i++) {
        if (!pmm_get_bit(i)) {
            found_pages += 1;
            if (first_page == 0) {
                first_page = i;
            }
        } else {
            found_pages = 0;
            first_page = 0;
        }

        if (found_pages == pages) {
            return first_page;
        }
    }

    sprintf("\n[PMM] Error! Couldn't find free memory!");
    while (1) {
        asm volatile("hlt");
    }

    return 0;
}

void *pmm_alloc(uint64_t size) {
    lock(pmm_lock);

    uint64_t pages = (size + 0x1000 - 1) / 0x1000;
    uint64_t free_page = find_free_page(pages);

    for (uint64_t i = 0; i < pages; i++) {
        pmm_set_bit(free_page + i);
    }

    available_memory -= pages * 0x1000;
    used_memory += pages * 0x1000;

    unlock(pmm_lock);
    return (void *) (free_page * 0x1000);
}

void pmm_unalloc(void *addr, uint64_t size) {
    lock(pmm_lock);

    uint64_t page = ((uint64_t) addr & ~(0xfff)) / 0x1000;
    uint64_t pages = (size + 0x1000 - 1) / 0x1000;

    for (uint64_t i = 0; i < pages; i++) {
        pmm_clear_bit(page + i);
    }

    available_memory += pages * 0x1000;
    used_memory -= pages * 0x1000;

    unlock(pmm_lock);
}

uint64_t pmm_get_free_mem() {
    return available_memory;
}

uint64_t pmm_get_used_mem() {
    return used_memory;
}

uint64_t pmm_get_total_mem() {
    return total_memory;
}