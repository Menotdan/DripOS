#include "pmm.h"
#include "../libc/mem.h"
#include "../drivers/screen.h"
#include "../drivers/serial.h"
#include "../cpu/timer.h"

/* 
 * This is calculated by memory map parsing.
 */
uint64_t new_addr = 0;
uint64_t prev_addr = 0;
uint64_t bitmap_size = 0;
uint8_t *bitmap = 0;
uint64_t free_mem_addr = 0;
uint64_t memory_remaining = 0;
uint64_t used_mem = 0;
uint64_t MAX = 0;
uint64_t MIN = 0;

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
    sprint_uint64((uint64_t)bitmap);
    memset(bitmap, 0, bitmap_size);
    free_mem_addr += bitmap_size;
    memory_remaining -= bitmap_size;

    free_mem_addr = (free_mem_addr + 0xFFF) & ~0xFFF;
    MIN = free_mem_addr;
    memory_remaining = MAX-MIN;
    sprint("\nFree addr: ");
    sprint_uint64(free_mem_addr);
    sprint("\nMIN: ");
    sprint_uint64(MIN);
    sprint("\nRemaining: ");
    sprint_uint64(memory_remaining);
}

/* Configure memory mapping and such */
void configure_mem(multiboot_info_t *mbd) {
        // Current mmap address
		uint64_t current = ((uint64_t)mbd->mmap_addr) & 0xffffffff;
		// Remaing mmap data
		uint64_t remaining = mbd->mmap_length;

		for (; remaining > 0; remaining -= sizeof(multiboot_memory_map_t)) {
			multiboot_memory_map_t *mmap = (multiboot_memory_map_t *)(current);

			char buf1[100];
			char buf2[100];
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
			memset((uint8_t *)buf1, 0, 100);
			memset((uint8_t *)buf2, 0, 100);
		}
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
        asm volatile("int $22");
    }
    return pointer;
}

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
    free_mem_addr += allocated_space;
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
    free_mem_addr -= allocated_space;
    used_mem -= allocated_space;
    memory_remaining += allocated_space;
}