#include "pmm.h"
#include "../libc/mem.h"
#include "../drivers/screen.h"
#include "../drivers/serial.h"
#include "../cpu/timer.h"

/* Some globals for the PMM to use as needed */
uint8_t *cur_map = 0;
uint64_t total_memory = 0;
uint64_t total_usable = 0;
uint64_t new_addr = 0;
uint64_t prev_addr = 0;
uint64_t memory_remaining = 0;
uint64_t used_mem = 0;
uint64_t MAX = 0;
uint64_t MIN = 0;
uint8_t *bitmap = 0;

/* Bitmap for use with the PMM. Actually a linked list of bitmaps where at the
   start of the bitmap, there is the size of the bitmap in bytes (8 byte field),
   and then there is the actual bitmap, which holds the info for what pages are
   and aren't allocated. Then finally there is the pointer to the next bitmap in
   the chain which is set to 0 if there is no bitmap after this one. 
   There is also 8 bytes at the very end used for storing the address this
   bitmap represents */

/* Get bit relative, gets a bit in a single uint8_t */
uint8_t get_bit_rel(uint8_t input, uint8_t bit) {
    return ((input >> bit) & 1);
}

/* Set bit relative, sets a bit in a single uint8_t */
void set_bit_rel(uint8_t *input, uint8_t bit, uint8_t state) {
    if (state == 0) {
        *input &= ~(1 << bit);
    } else if (state == 1)  {
        *input = (*input | (1 << bit));
    }
}

/* Get a bit in the bitmap */
uint8_t get_bit(uint8_t *bitmap_change, uint8_t bit, uint64_t byte) {
    return ((*(bitmap_change + byte + 8) >> bit) & 1);
}

/* Set a bit in the bitmap */
void set_bit(uint8_t *bitmap_change, uint8_t bit, uint64_t byte, uint8_t state) {
    if (state == 0) {
        *(bitmap_change + byte + 8) &= ~(1 << bit);
    } else if (state == 1)  {
        *(bitmap_change + byte + 8) = (*(bitmap_change + byte + 8) | (1 << bit));
    }
}

/* Get the size of the bitmap */
uint64_t get_bitmap_size(uint8_t *bitmap_start) {
    return *(uint64_t *) bitmap_start; // Get the size data
}

/* Get the address that a bitmap represents */
uint64_t get_represented_addr(uint8_t *bitmap_start) {
    if (!bitmap_start) {
        return (uint64_t) 0; // Return null
    }

    return *(uint64_t *) ((*(uint64_t *) bitmap_start) + bitmap_start + 8);
}

/* Get next bitmap in a chain of bitmaps */
uint8_t *get_next_bitmap(uint8_t *bitmap_start) {
    if (!bitmap_start) {
        return (uint8_t *) 0; // Return null
    }

    uint8_t *bitmap_cur = bitmap_start;
    bitmap_cur += *(uint64_t *)bitmap_start; // Jump to the end of the map
    return (uint8_t *) *(uint64_t *)bitmap_cur;
}

/* Get the last bitmap in a chain of bitmaps */
uint8_t *get_last_bitmap(uint8_t *bitmap_start) {
    uint8_t *ret = 0;
    uint8_t *cur_map = bitmap_start;
    
    while (cur_map != 0) {
        ret = cur_map;
        cur_map = get_next_bitmap(cur_map);
    }

    return ret;
}

/* Setup a bitmap which may be pointed to by another bitmap */
void set_bitmap(uint8_t *bitmap_start, uint8_t *old_bitmap, uint64_t size_of_mem, uint64_t offset) {
    // Calculate the bitmap size in bytes, we add 8 to account for the size data
    uint64_t bitmap_size = (((size_of_mem) + (0x1000 * 8) - 1) / (0x1000 * 8)) + 8;
    // Number of pages needed for the bitmap
    uint64_t bitmap_pages = ((bitmap_size + 16) + 0x1000 - 1) / 0x1000;
    bitmap_pages += ((offset + 0x1000 - 1) / 0x1000);

    if (old_bitmap) { // If old_bitmap is not 0
        // Set the bitmap pointer for the old bitmap to the new one
        *(uint64_t *) ((uint64_t) old_bitmap + *(uint64_t *) old_bitmap) = (uint64_t) bitmap_start;
    }

    // Clear the bitmap, including the pointer
    memset(bitmap_start, 0, (bitmap_size) + 16);
    // Set the size, not including the pointer
    *(uint64_t *) bitmap_start = (bitmap_size);
    // Set the represented address of the bitmap
    *(uint64_t *) (bitmap_start + bitmap_size + 8) = (uint64_t)bitmap_start - offset;
    /* Debug data
    sprint("\nPages: ");
    sprint_hex(bitmap_pages);
    sprint("\nBitmap size: ");
    sprint_hex(bitmap_size);
    sprint("\nMem size: ");
    sprint_hex(size_of_mem);
    sprint("\nOffset: ");
    sprint_hex(offset);
    sprint("\nPages without offset: ");
    sprint_hex(((bitmap_size + 16) + 0x1000 - 1) / 0x1000);
    */
    
    // Now to set the bitmap as allocated
    uint64_t bitmap_bit = 0;
    uint64_t bitmap_byte = 0;
    for (; bitmap_pages > 0; bitmap_pages--) {
        set_bit(bitmap_start, bitmap_bit, bitmap_byte, 1);
        
        bitmap_bit++;
        if (bitmap_bit == 8) {
            bitmap_bit = 0;
            bitmap_byte++;
        }
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
        /* Bitmap setup for use with the initial page table setup */
        if (mmap->type == MULTIBOOT_MEMORY_AVAILABLE) {
            sprint("Usable");

            total_usable += mmap->len;

            if (remaining == mbd->mmap_length) {
                // Found lower 640K of memory, ignore it, as to not overwrite stuff
            } else {
                // Check if the kernel is in this block
                sprint("\nAddress: ");
                sprint_hex(mmap->addr);
                sprint("\nKernel start: ");
                sprint_hex((uint64_t) __kernel_start - KERNEL_VMA_OFFSET);
                sprint("\nKernel end: ");
                sprint_hex((uint64_t)__kernel_end - KERNEL_VMA_OFFSET);
                if ((uint64_t) __kernel_start - KERNEL_VMA_OFFSET >= (mmap->addr) && 
                ((uint64_t) __kernel_end - KERNEL_VMA_OFFSET) < (mmap->len + mmap->addr) 
                - ((mmap->len + mmap->addr) / 0x1000 / 8)) {
                    // The kernel is here and so we setup the bitmap
                    bitmap = (uint8_t *) (((uint64_t) __kernel_end + 0x1000) & ~(0xfff));
                    set_bitmap(bitmap, 0, mmap->len, (((uint64_t) __kernel_end + 0x1000 - KERNEL_VMA_OFFSET) & ~(0xfff)) - (mmap->addr));
                    sprint("\nFirst bitmap set!");
                    break;
                }
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
    sprint("Finding memory...");
    sprint_hex(pmm_find_free(1) - KERNEL_VMA_OFFSET);
}

uint64_t pmm_find_free(uint64_t size) {
    // Get bitmap data for the first bitmap
    cur_map = bitmap;
    uint64_t bitmap_size = get_bitmap_size(cur_map);

    // Go through all the bitmaps
    while (cur_map != 0) {
        uint64_t found_free_bits = 0;
        uint64_t first_addr = 0;
        // For each byte in the bitmap
        for (uint64_t bytes_iterated = 0; bytes_iterated < bitmap_size; bytes_iterated++) {
            // For each bit in the byte
            for (uint64_t bits_iterated = 0; bits_iterated < 8; bits_iterated++) {
                // If the bit here is 0
                if (!get_bit(cur_map, bits_iterated, bytes_iterated)) {
                    found_free_bits += 1;
                    if (!first_addr) {
                        first_addr = (get_represented_addr(cur_map) + (((bytes_iterated * 8) + (bits_iterated)) * 0x1000));
                    }
                } else {
                    found_free_bits = 0;
                    first_addr = 0;
                }
                // Found enough space
                if (found_free_bits == size) {
                    return first_addr;
                }
            }
        }

        // Get bitmap data for the next bitmap
        cur_map = get_next_bitmap(cur_map);
        if (cur_map != 0) {
            bitmap_size = get_bitmap_size(cur_map);
        } else {
            break;
        }
    }
    return 0; // Nothing found
}

uint64_t pmm_allocate(uint64_t size) {
    uint64_t needed = ((size + 0x1000) & ~(0xfff)) / 0x1000; // Calculate needed pages
    uint64_t free_addr = pmm_find_free(needed); // Find the pages in bitmap land

    if (!free_addr) {
        return 0; // Nothing found, say oof
    }

    /* Set pages as allocated */

    // Difference between the start of the memory represented by the map and the found addr
    uint64_t difference = free_addr - get_represented_addr(cur_map);
    difference /= 0x1000;

    return free_addr - KERNEL_VMA_OFFSET; // yey we found something :D
}

void pmm_unallocate(void * address, uint64_t size) {
    if (address) {}
    if (size) {}
}