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

lock_t pmm_lock = 0;

void pmm_set_bit(uint64_t page) {
    uint8_t bit = page & 0b111;
    uint64_t byte = page & ~(0b111);
    //sprintf("\npage: %lu %lu %u", page, byte, (uint32_t) bit);

    bitmap[byte] |= (1<<bit);
}

void pmm_clear_bit(uint64_t page) {
    uint8_t bit = page & 0b111;
    uint64_t byte = page & ~(0b111);

    bitmap[byte] &= ~(1<<bit);
}

uint8_t pmm_get_bit(uint64_t page) {
    uint8_t bit = page & 0b111;
    uint64_t byte = page & ~(0b111);

    return bitmap[byte] & (1<<bit);
}

void pmm_memory_setup(multiboot_info_t *mboot_dat) {
    // Bitmap set to kernel_end rounded up to a page
    bitmap = (uint8_t *) ((((uint64_t) __kernel_end) + 0x1000 - 1) & ~(0xfff));

    // Setup the bitmap for the PMM
    multiboot_memory_map_t *mmap = GET_HIGHER_HALF(multiboot_memory_map_t *, mboot_dat->mmap_addr);
    // Calculate length of memory
    multiboot_memory_map_t start = mmap[0];
    multiboot_memory_map_t end = mmap[(mboot_dat->mmap_length / sizeof(multiboot_memory_map_t)) - 1];
    uint64_t mem_length = ROUND_UP((end.addr + end.len) - start.addr, 0x1000);
    uint64_t bitmap_bytes = ((mem_length / 0x1000) + 8 - 1) / 8;

    // Set bitmap as 1s
    memset(bitmap, 0xFF, bitmap_bytes);

    for (uint64_t i = 0; i < mboot_dat->mmap_length / sizeof(multiboot_memory_map_t); i++) {
        uint64_t rounded_block_start = ROUND_UP(mmap[i].addr, 0x1000);
        uint64_t rounded_block_end = ROUND_UP((mmap[i].addr + mmap[i].len), 0x1000);
        uint64_t rounded_block_len = rounded_block_end - rounded_block_start;

        if (mmap[i].type == MULTIBOOT_MEMORY_AVAILABLE && i != 0) { // Ignore low 640K so we dont use it
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

    vmm_map((void *) 0, GET_HIGHER_HALF(void *, 0), bitmap_max_page, VMM_PRESENT | VMM_WRITE);

    base_kernel_cr3 = vmm_get_pml4t();
}

uint64_t find_free_page(uint64_t pages) {
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
    lock(&pmm_lock);

    uint64_t pages = (size + 0x1000 - 1) / 0x1000;
    uint64_t free_page = find_free_page(pages);

    for (uint64_t i = 0; i < pages; i++) {
        pmm_set_bit(free_page + i);
    }

    available_memory -= pages * 0x1000;
    used_memory += pages * 0x1000;

    unlock(&pmm_lock);
    return (void *) (free_page * 0x1000);
}

void pmm_unalloc(void *addr, uint64_t size) {
    lock(&pmm_lock);

    uint64_t page = ((uint64_t) addr & ~(0xfff)) / 0x1000;
    uint64_t pages = (size + 0x1000 - 1) / 0x1000;

    for (uint64_t i = 0; i < pages; i++) {
        pmm_clear_bit(page + i);
    }

    available_memory += pages * 0x1000;
    used_memory -= pages * 0x1000;

    unlock(&pmm_lock);
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