#include "pmm.h"
#include "../libc/mem.h"
#include "../drivers/screen.h"
#include "../drivers/serial.h"
#include "../cpu/timer.h"

/* 
 * This is calculated by memory map parsing.
 */
pmm_usable_list_t usable_mem;
uint64_t total_memory = 0;
uint64_t total_usable = 0;
uint64_t new_addr = 0;
uint64_t prev_addr = 0;
uint64_t bitmap_size = 0;
uint64_t memory_remaining = 0;
uint64_t used_mem = 0;
uint64_t MAX = 0;
uint64_t MIN = 0;
uint8_t *bitmap = 0;

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

/* Configure memory mapping and such */
void configure_mem(multiboot_info_t *mbd) {
    // Current mmap address
    uint64_t current = ((uint64_t)mbd->mmap_addr) & 0xffffffff;
    // Remaining mmap data
    uint64_t remaining = mbd->mmap_length;

    total_memory += (mbd->mem_lower) * 1024;
    total_memory += (mbd->mem_upper) * 1024;

    for (; remaining > 0; remaining -= sizeof(multiboot_memory_map_t)) {
        multiboot_memory_map_t *mmap = (multiboot_memory_map_t *)(current);

        char buf1[19];
        char buf2[19];
        uint64_t start = mmap->addr;
        uint64_t end = start + mmap->len;
        htoa(start, buf1);
        htoa(end, buf2);
        
        sprint("\nEntry:\n  ");
        sprint(buf1);
        sprint(" - ");
        sprint(buf2);
        sprint("\n  Type: ");
        
        if (mmap->type == MULTIBOOT_MEMORY_AVAILABLE) {
            sprint("Usable");

            total_usable += mmap->len;

            if (remaining == mbd->mmap_length) {
                // Found the lower 640K of memory
                usable_mem.address = mmap->addr;
                usable_mem.size = mmap->len & ~(0xFFF);
                usable_mem.next = 0;
            }
        } else if (mmap->type == MULTIBOOT_MEMORY_RESERVED) {
            sprint("Reserved");
        } else if (mmap->type == MULTIBOOT_MEMORY_ACPI_RECLAIMABLE) {
            sprint("ACPI Reclaimable");
        } else if (mmap->type == MULTIBOOT_MEMORY_NVS) {
            sprint("NVS");
        } else if (mmap->type == MULTIBOOT_MEMORY_BADRAM) {
            sprint("Bad memory");
        }

        current += sizeof(multiboot_memory_map_t);
        memset((uint8_t *)buf1, 0, 19);
        memset((uint8_t *)buf2, 0, 19);
    }
    /* Setup the bitmap for the pmm allocator */
    total_usable &= ~((uint64_t)0xFFF); // Round the amount of memory down

}

uint64_t pmm_find_free(uint64_t size) {
    uint64_t pages_needed = size/0x1000;
    uint64_t number_of_free = 0;
    uint64_t pointer = 0;
    uint64_t bitmap_byte = 0;
    uint64_t bitmap_bit = 0;
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
        asm volatile("int $22"); // Out of memory :/
    }
    return pointer;
}

// void *pmm_find(uint64_t pages) {

// }

// void *pmm_alloc(uint64_t size) {

// }

uint64_t pmm_allocate(uint64_t size) {
    uint64_t ret = pmm_find_free(size);
    if (ret == 0 || ret < MIN || ret > MAX) {
        return 0;
    }
 
    uint64_t pages_needed = size/0x1000;
    uint64_t bitmap_byte = 0; // The first byte in the sequence
    uint64_t bitmap_bit = 0; // The first bit in the sequence
    uint8_t *cur_bitmap_pos = bitmap;
    uint64_t offset = ret - MIN; // Offset from the beginning of free memory
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
    used_mem += allocated_space;
    memory_remaining -= allocated_space;

    return ret;
}

void pmm_unallocate(void * address, uint64_t size) {
    uint64_t ret = (uint64_t)address;
    if (ret == 0 || ret < MIN || ret > MAX) {
        return;
    }
 
    uint64_t pages_needed = size/0x1000;
    uint64_t bitmap_byte = 0; // The first byte in the sequence
    uint64_t bitmap_bit = 0; // The first bit in the sequence
    uint8_t *cur_bitmap_pos = bitmap;
    uint64_t offset = ret - MIN; // Offset from the beginning of free memory
    if (size % 0x1000) {
        pages_needed += 1;
    }

    bitmap_byte = (offset/0x1000)/8; // Get the byte to start writing to the bitmap
    bitmap_bit = (offset/0x1000)%8; // Get the bit to start writing to the bitmap
    cur_bitmap_pos += bitmap_byte;
    for (uint64_t i = 0; i < pages_needed; i++) {
        set_bit(cur_bitmap_pos, bitmap_bit, 0);
        bitmap_bit++;
        if (bitmap_bit == 8) {
            bitmap_bit = 0;
            bitmap_byte++;
            cur_bitmap_pos++;
        }
    }

    sprint("\nFreed ");
    sprint_uint64(size);
    sprint(" bytes and ");
    sprint_uint64(pages_needed);
    sprint(" pages and ");
    sprint_uint64(pages_needed*0x1000);
    sprint(" bytes were logged\nAddress: ");
    sprint_uint64((uint64_t)address);
    uint64_t allocated_space = pages_needed*0x1000;
    used_mem -= allocated_space;
    memory_remaining += allocated_space;
}