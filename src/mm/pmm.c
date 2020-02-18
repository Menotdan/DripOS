#include "pmm.h"
#include "vmm.h"
#include "io/ports.h"
#include "klibc/string.h"
#include "klibc/lock.h"
#include "drivers/serial.h"
#include "drivers/tty/tty.h"

/* An implementation for managing a list of bitmaps that looks like this:
    bitmap_size | bitmap | bitmap_represented | next_entry
     -> bitmap_size | bitmap | bitmap_represented | next_entry */

uint8_t *base_bitmap; // The base bitmap in the list
uint8_t *cur_map; // The current map last found by the free space finder
uint64_t usable_mem; // The estimated amount of usable memory
uint64_t used_mem; // The estimated amount of used memory

lock_t pmm_spinlock = 0; // The spinlock for the PMM

/* Get a bit in the bitmap */
uint8_t pmm_get_bit(uint8_t *bitmap_change, uint8_t bit, uint64_t byte) {
    uint8_t ret = 0;
    //spinlock_lock(&pmm_spinlock);
    ret = ((*(bitmap_change + byte + 8) >> bit) & 1);
    //spinlock_unlock(&pmm_spinlock);
    return ret;
}

/* Set a bit in the bitmap */
void pmm_set_bit(uint8_t *bitmap_change, uint8_t bit, uint64_t byte, uint8_t state) {
    //spinlock_lock(&pmm_spinlock); // Lock the PMM
    if (state == 0) {
        *(bitmap_change + byte + 8) &= ~(1 << bit);
    } else if (state == 1) {
        *(bitmap_change + byte + 8) = (*(bitmap_change + byte + 8) | (1 << bit));
    }
    //spinlock_unlock(&pmm_spinlock); // Unlock the PMM
}

/* Get the size of the bitmap from it's pointer */
uint64_t pmm_get_bitmap_size(uint8_t *bitmap_start) {
    return *(uint64_t *)bitmap_start; // Get the size data
}

/* Get the address that a bitmap represents */
uint64_t pmm_get_represented_addr(uint8_t *bitmap_start) {
    if (!bitmap_start) {
        return (uint64_t)0; // Return null
    }
    uint64_t bitmap_size = pmm_get_bitmap_size(bitmap_start);
    return *(uint64_t *)(bitmap_size + bitmap_start + 8);
}

/* Get next bitmap in a chain of bitmaps */
uint8_t *pmm_get_next_bitmap(uint8_t *bitmap_start) {
    if (!bitmap_start) {
        return (uint8_t *)0; // Return null
    }

    uint64_t bitmap_size = pmm_get_bitmap_size(bitmap_start);
    uint8_t *bitmap_cur = bitmap_start;
    bitmap_cur += bitmap_size; // Go to the end of the bitmap

    uint64_t next_bitmap_pointer = *(uint64_t *)bitmap_cur;
    return (uint8_t *) next_bitmap_pointer;
}

/* Get the last bitmap in a chain of bitmaps */
uint8_t *pmm_get_last_bitmap(uint8_t *bitmap_start) {
    uint8_t *ret = 0;
    uint8_t *cur_map = bitmap_start;

    while (cur_map != 0) {
        ret = cur_map;
        cur_map = pmm_get_next_bitmap(cur_map);
    }

    return ret;
}

/* Setup a bitmap which may be pointed to by another bitmap */
void pmm_set_bitmap(uint8_t *bitmap_start, uint8_t *old_bitmap, uint64_t size_of_mem, uint64_t offset) {
    spinlock_lock(&pmm_spinlock);
    uint8_t *real_bitmap_pos = bitmap_start + offset;
    uint64_t *size_data_writing = (uint64_t *) real_bitmap_pos;
    uint64_t bitmap_bytes = (size_of_mem + (0x1000 * 8) - 1) / (0x1000 * 8); // Bitmap size in bytes
    bitmap_bytes += 8; // To account for the bitmap size data stored in the bitmap itself
    memset(real_bitmap_pos, 0, bitmap_bytes + 16); // Clear the whole bitmap, including the size data and other data

    *size_data_writing = bitmap_bytes;

    if (old_bitmap) { // If old_bitmap is not null
        // Have the old bitmap point to the new one
        uint64_t old_bitmap_size = pmm_get_bitmap_size(old_bitmap);
        uint64_t *old_bitmap64 = (uint64_t *) (old_bitmap + old_bitmap_size);
        *old_bitmap64 = (uint64_t) bitmap_start;
    }

    *(uint64_t *) (real_bitmap_pos + bitmap_bytes + 8) = (uint64_t) bitmap_start;
    // Finally, mark the bitmap + the offset as used on the bitmap
    uint64_t bitmap_size = bitmap_bytes + 16 + offset;
    uint64_t bitmap_pages = (bitmap_size + 0x1000 - 1) / 0x1000; // Calculate the bitmap size in pages, rounded up

    uint64_t bitmap_byte = 0;
    uint8_t bitmap_bit = 0;
    for (uint64_t i = 0; i < bitmap_pages; i++) {
        pmm_set_bit(real_bitmap_pos, bitmap_bit, bitmap_byte, 1); // Mark every page the bitmap uses in the bitmap as used

        bitmap_bit++;
        if (bitmap_bit == 8) {
            bitmap_byte++;
            bitmap_bit = 0;
        }
    }
    spinlock_unlock(&pmm_spinlock);
}

bitmap_index pmm_get_bitmap(void *addr) {
    bitmap_index ret;
    uint64_t new_addr = (uint64_t) addr & ~(0xfff);
    cur_map = base_bitmap;

    ret.bit = 0;
    ret.byte = 0;

    while (cur_map) {
        uint64_t represented = pmm_get_represented_addr(cur_map); // begining of the represented addresses by the map
        uint64_t represented_end = represented + ((pmm_get_bitmap_size(cur_map) - 8) * 0x1000 * 8); // end of the bitmap

        if (new_addr >= represented && new_addr <= represented_end) {
            uint64_t offset = new_addr - represented;
            uint64_t offset_pages = (offset + 0x1000 - 1) / 0x1000;
            ret.byte = offset_pages / 8;
            ret.bit = offset_pages % 8;
            return ret;
        }

        cur_map = pmm_get_next_bitmap(cur_map);
    }

    return ret;
}

uint64_t pmm_get_free_mem() {
    uint64_t ret = 0;
    spinlock_lock(&pmm_spinlock); // Wait for us to have the lock so we can get accurate size readings
    ret = usable_mem; // Store the value
    spinlock_unlock(&pmm_spinlock); // Unlock it 
    return ret;
}

uint64_t pmm_get_used_mem() {
    uint64_t ret = 0;
    spinlock_lock(&pmm_spinlock); // Wait for us to have the lock se we can get accurate size readings
    ret = used_mem;
    spinlock_unlock(&pmm_spinlock);
    return ret;
}

void pmm_memory_setup(multiboot_info_t *mboot_dat) {
    // Current mmap address
    uint64_t current = ((uint64_t)mboot_dat->mmap_addr) & 0xffffffff;
    // Remaining mmap data
    uint64_t remaining = mboot_dat->mmap_length;

    // Iterate over the map for the first time
    for (; remaining > 0; remaining -= sizeof(multiboot_memory_map_t)) {
        multiboot_memory_map_t *mmap = (multiboot_memory_map_t *)(current);
        /* Bitmap setup for use with the initial page table setup */
        if (mmap->type == MULTIBOOT_MEMORY_AVAILABLE) {
            if (mmap->addr == 0 || remaining == mboot_dat->mmap_length) {
                // Found lower 640K of memory, ignore it, as to not overwrite stuff
            } else {
                // Is this the kernel block?
                uint64_t kernel_start = ((uint64_t) __kernel_start) - KERNEL_VMA_OFFSET; // Find the non offset kernel start
                uint64_t kernel_end = ((uint64_t) __kernel_end) - KERNEL_VMA_OFFSET; // Find the non offset kernel end
                uint64_t block_start = mmap->addr & ~(0xfff); // Round down the block start to a page
                uint64_t block_end = (mmap->len + mmap->addr) & ~(0xfff); // Round down the block end to a page
                uint64_t block_len = block_end - block_start; // Calculate the rounded block length
                if (kernel_start >= block_start && kernel_end <= block_end) {
                    // This is indeed the kernel block
                    base_bitmap = (uint8_t *) (kernel_end + NORMAL_VMA_OFFSET);
                    uint64_t bitmap_offset = kernel_end - block_start;
                    pmm_set_bitmap(base_bitmap - bitmap_offset, 0, block_len, bitmap_offset);
                    break;
                }
            }
        }
        current += sizeof(multiboot_memory_map_t);
    }

    /* Reset vars */
    // Current mmap address
    current = ((uint64_t)mboot_dat->mmap_addr) & 0xffffffff;
    // Remaining mmap data
    remaining = mboot_dat->mmap_length;
    // Iterate over the map for the first time
    for (; remaining > 0; remaining -= sizeof(multiboot_memory_map_t)) {
        multiboot_memory_map_t *mmap = (multiboot_memory_map_t *)(current);
        /* Bitmap setup for use with the initial page table setup */
        if (mmap->type == MULTIBOOT_MEMORY_AVAILABLE) {
            // Is this the kernel block?
            uint64_t kernel_start = ((uint64_t) __kernel_start) - KERNEL_VMA_OFFSET; // Find the non offset kernel start
            uint64_t kernel_end = ((uint64_t) __kernel_end) - KERNEL_VMA_OFFSET; // Find the non offset kernel end
            uint64_t block_start = mmap->addr & ~(0xfff); // Round down the block start to a page
            uint64_t block_end = (mmap->len + mmap->addr) & ~(0xfff); // Round down the block end to a page
            uint64_t block_len = block_end - block_start; // Calculate the rounded block length
            uint64_t bitmap_len = (block_len + (0x1000 * 8) - 1) / (0x1000 * 8);
            bitmap_len += 16; // to account for size data and such
            if (kernel_start >= block_start && kernel_end <= block_end) {
                // This is indeed the kernel block, remap it
                vmm_remap((void *) block_start, (void *) (block_start + NORMAL_VMA_OFFSET), (block_len / 0x1000), 
                    VMM_PRESENT | VMM_WRITE);
            } else {
                if (mmap->addr != 0) {
                    // Map enough pages for a bitmap
                    uint64_t pages_mapped = 0;
                    vmm_map((void *) block_start, (void *) (block_start + NORMAL_VMA_OFFSET), 
                        (bitmap_len + 0x1000 - 1) / 0x1000,
                        VMM_PRESENT | VMM_WRITE);
                    pages_mapped += ((block_len / (0x1000 * 8)) + 0x1000 - 1) / 0x1000;
                    /* Set a bitmap for that block */
                    pmm_set_bitmap((uint8_t *) block_start + NORMAL_VMA_OFFSET, base_bitmap, block_len, 0);
                    /* Map the rest of the memory */
                    vmm_map((void *) (block_start + (pages_mapped * 0x1000)), 
                        (void *) (block_start + (pages_mapped * 0x1000) + NORMAL_VMA_OFFSET),
                        (block_len / 0x1000) - pages_mapped, 
                        VMM_PRESENT | VMM_WRITE);
                } else {
                    vmm_map((void *) block_start, (void *) (block_start + NORMAL_VMA_OFFSET), (block_len / 0x1000), 
                        VMM_PRESENT | VMM_WRITE);
                }
            }
        } else {
            uint64_t block_start = mmap->addr & ~(0xfff); // Round down the block start to a page
            uint64_t block_end = (mmap->len + mmap->addr) & ~(0xfff); // Round down the block end to a pag
            uint64_t block_len = block_end - block_start; // Calculate the rounded block length

            vmm_map((void *) block_start, (void *) (block_start + NORMAL_VMA_OFFSET), (block_len / 0x1000), 
                VMM_PRESENT | VMM_WRITE);
        }
        current += sizeof(multiboot_memory_map_t);
    }
}

bitmap_index pmm_find_free(uint64_t pages) {
    bitmap_index ret;
    cur_map = base_bitmap;
    uint64_t bitmap_size;

    while (cur_map) {
        uint64_t found = 0;
        uint64_t found_index_byte = 0;
        uint8_t found_index_bit = 0;

        bitmap_size = pmm_get_bitmap_size(cur_map) - 8; // Minus 8 to remove the start data
        for (uint64_t byte = 0; byte < bitmap_size; byte++) {
            for (uint8_t bit = 0; bit < 8; bit++) {
                if (!pmm_get_bit(cur_map, bit, byte)) {
                    if (!found) {
                        found_index_bit = bit;
                        found_index_byte = byte;
                    }
                    found++;

                    if (found == pages) {
                        ret.bit = found_index_bit;
                        ret.byte = found_index_byte;
                        return ret;
                    }
                }
            }
        }
        cur_map = pmm_get_next_bitmap(cur_map);
    }
    /* if we didn't find anything, set cur_map to 0 and return */
    cur_map = 0;
    ret.bit = 0;
    ret.byte = 0;
    return ret;
}

void *pmm_alloc(uint64_t size) {
    spinlock_lock(&pmm_spinlock);
    //sprintf("\nAllocating %lu", size);
    uint64_t pages_needed = (size + 0x1000 - 1) / 0x1000;
    bitmap_index free_space = pmm_find_free(pages_needed);

    if (cur_map) {
        uint64_t byte = free_space.byte;
        uint8_t bit = free_space.bit;

        for (uint64_t i = 0; i < pages_needed; i++) {
            pmm_set_bit(cur_map, bit, byte, 1); // Set all the bits as allocated

            bit++;
            if (bit == 8) {
                byte++;
                bit = 0;
            }
        }
        uint64_t free_space_offset = ((free_space.byte * 0x1000 * 8) + (free_space.bit * 0x1000));
        void *ret = (void *) (pmm_get_represented_addr(cur_map) + free_space_offset - NORMAL_VMA_OFFSET);
        spinlock_unlock(&pmm_spinlock);
        return ret;
    } else {
        sprintf("\n[PMM] Warning: couldn't find free space for size %lx", size);
        spinlock_unlock(&pmm_spinlock);
        return (void *) 0;
    }
}

void pmm_unalloc(void *addr, uint64_t size) {
    spinlock_lock(&pmm_spinlock);
    uint64_t pages_needed = (size + 0x1000 - 1) / 0x1000;
    uint64_t virt_addr = (uint64_t) addr + NORMAL_VMA_OFFSET;
    bitmap_index to_free = pmm_get_bitmap((void *) virt_addr);

    if (cur_map) {
        uint64_t byte = to_free.byte;
        uint8_t bit = to_free.bit;
        
        for (uint64_t i = 0; i < pages_needed; i++) {
            pmm_set_bit(cur_map, bit, byte, 0); // Set all the bits as unallocated

            bit++;
            if (bit == 8) {
                byte++;
                bit = 0;
            }
        }
    } else {
        sprintf("\n[PMM] Warning: couldn't find bitmap for address %lx", addr);
    }
    spinlock_unlock(&pmm_spinlock);
}