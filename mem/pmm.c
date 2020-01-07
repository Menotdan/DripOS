#include "pmm.h"
#include "../libc/mem.h"
#include "../drivers/screen.h"
#include "../drivers/serial.h"
#include "../cpu/timer.h"

/* 
 * This is calculated by memory map parsing.
 */
uint32_t new_addr = 0;
uint32_t prev_addr = 0;
uint32_t bitmap_size = 0;
uint8_t *bitmap = 0;
uint32_t free_mem_addr = 0;
uint32_t memory_remaining = 0;
uint32_t used_mem = 0;
uint32_t MAX = 0;
uint32_t MIN = 0;

uint8_t get_bit(uint8_t input, uint8_t bit) {
    return ((input >> bit) & 1);
}

void set_bit(uint8_t *input, uint8_t bit, uint8_t state) {
    if (state == 0) {
        *input &= ~(1 << bit);
    } else if (state == 1)  {
        *input = (*input | (1 << bit));
    }
}

void set_addr(uint32_t addr, uint32_t mem_size) {
    free_mem_addr = addr;
    memory_remaining = mem_size;
    MAX = mem_size + addr;
    MIN = free_mem_addr;
    sprint("\n Setting memory variables... \n  ");
    sprint_uint(addr);
    sprint("\n   ");
    sprint_uint(mem_size);
    sprint("\n");
    sprint("\nSetting up memory bitmap...");
    bitmap = (uint8_t *)free_mem_addr;
    bitmap_size = (mem_size / 0x1000) / 8;
    MIN += bitmap_size;
    sprint("\nNumber of bytes needed for bitmap: ");
    sprint_uint(bitmap_size);
    sprint("\nBitmap address: ");
    sprint_uint((uint32_t)bitmap);
    memset(bitmap, 0, bitmap_size);
    free_mem_addr += bitmap_size;
    memory_remaining -= bitmap_size;

    free_mem_addr = (free_mem_addr + 0xFFF) & ~0xFFF;
    MIN = free_mem_addr;
    memory_remaining = MAX-MIN;
    sprint("\nFree addr: ");
    sprint_uint(free_mem_addr);
    sprint("\nMIN: ");
    sprint_uint(MIN);
    sprint("\nRemaining: ");
    sprint_uint(memory_remaining);
}

uint32_t pmm_find_free(uint32_t size) {
    uint32_t pages_needed = size/0x1000;
    uint32_t number_of_free = 0;
    uint32_t pointer = 0;
    uint32_t bitmap_byte = 0;
    uint32_t bitmap_bit = 0;
    if (size % 0x1000) {
        pages_needed += 1;
    }
    for (uint32_t i = 0; i < bitmap_size*8; i++) {
        if (get_bit(*(bitmap+bitmap_byte), bitmap_bit) == 0) {
            if (number_of_free == 0) {
                pointer = (((bitmap_byte * 8) + bitmap_bit) * 0x1000)+MIN;
            }
            number_of_free++;
            if (number_of_free == pages_needed) {
                break;
            }
        } else {
            number_of_free = 0;
        }
        // Increment the counter
        bitmap_bit++;
        if (bitmap_bit == 8) {
            bitmap_byte++;
            bitmap_bit = 0;
        }
    }
    if (pointer == 0) {
        asm volatile("int $22");
    }
    return pointer;
}

uint32_t pmm_allocate(uint32_t size) {
    uint32_t ret = pmm_find_free(size);
    if (ret == 0 || ret < MIN || ret > MAX) {
        return 0;
    }
 
    uint32_t pages_needed = size/0x1000;
    uint32_t bitmap_byte = 0; // The first byte in the sequence
    uint32_t bitmap_bit = 0; // The first bit in the sequence
    uint8_t *cur_bitmap_pos = bitmap;
    uint32_t offset = ret - MIN; // Offset from the beginning of free memory
    if (size % 0x1000) {
        pages_needed += 1;
    }

    bitmap_byte = (offset/0x1000)/8; // Get the byte to start writing to the bitmap
    bitmap_bit = (offset/0x1000)%8; // Get the bit to start writing to the bitmap
    cur_bitmap_pos += bitmap_byte;
    for (uint32_t i = 0; i < pages_needed; i++) {
        set_bit(cur_bitmap_pos, bitmap_bit, 1);
        bitmap_bit++;
        if (bitmap_bit == 8) {
            bitmap_bit = 0;
            bitmap_byte++;
            cur_bitmap_pos++;
        }
    }
    sprint("\nAllocated ");
    sprint_uint(size);
    sprint(" bytes and ");
    sprint_uint(pages_needed);
    sprint(" pages and ");
    sprint_uint(pages_needed*0x1000);
    sprint(" bytes were logged\nAddress: ");
    sprint_uint(ret);
    uint32_t allocated_space = pages_needed*0x1000;
    free_mem_addr += allocated_space;
    used_mem += allocated_space;
    memory_remaining -= allocated_space;

    return ret;
}

void pmm_unallocate(void * address, uint32_t size) {
    uint32_t ret = (uint32_t)address;
    if (ret == 0 || ret < MIN || ret > MAX) {
        return;
    }
 
    uint32_t pages_needed = size/0x1000;
    uint32_t bitmap_byte = 0; // The first byte in the sequence
    uint32_t bitmap_bit = 0; // The first bit in the sequence
    uint8_t *cur_bitmap_pos = bitmap;
    uint32_t offset = ret - MIN; // Offset from the beginning of free memory
    if (size % 0x1000) {
        pages_needed += 1;
    }

    bitmap_byte = (offset/0x1000)/8; // Get the byte to start writing to the bitmap
    bitmap_bit = (offset/0x1000)%8; // Get the bit to start writing to the bitmap
    cur_bitmap_pos += bitmap_byte;
    for (uint32_t i = 0; i < pages_needed; i++) {
        set_bit(cur_bitmap_pos, bitmap_bit, 0);
        bitmap_bit++;
        if (bitmap_bit == 8) {
            bitmap_bit = 0;
            bitmap_byte++;
            cur_bitmap_pos++;
        }
    }

    sprint("\nFreed ");
    sprint_uint(size);
    sprint(" bytes and ");
    sprint_uint(pages_needed);
    sprint(" pages and ");
    sprint_uint(pages_needed*0x1000);
    sprint(" bytes were logged\nAddress: ");
    sprint_uint((uint32_t)address);
    uint32_t allocated_space = pages_needed*0x1000;
    free_mem_addr -= allocated_space;
    used_mem -= allocated_space;
    memory_remaining += allocated_space;
}