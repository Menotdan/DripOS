#include "vmm.h"
#include "../drivers/serial.h"

uint64_t current_pml4t = 0;

uint64_t vmm_get_pml4t() {
    uint64_t ret;
    asm volatile("movq %%cr3, %0;":"=r"(ret));
    return ret;
}

void vmm_set_pml4t(uint64_t new) {
    asm volatile("movq %0, %%cr3;"::"r"(new) : "memory");
}

// void vmm_flush_addr(uint64_t address) {

// }

void vmm_flush_tlb() {
    vmm_set_pml4t(vmm_get_pml4t());
}

pt_off_t vmm_virt_to_offs(void *virt) {
    //sprintf("\nVIRT: %lx", virt);
    uintptr_t addr = (uintptr_t)virt;

    pt_off_t off = {
        .p4_off =    (addr & ((size_t) 0x1ff << 39)) >> 39,
        .p3_off =    (addr & ((size_t) 0x1ff << 30)) >> 30,
        .p2_off =    (addr & ((size_t) 0x1ff << 21)) >> 21,
        .p1_off =    (addr & ((size_t) 0x1ff << 12)) >> 12,
    };

    return off;
}

void *vmm_offs_to_virt(pt_off_t offs) {
    uintptr_t addr = 0;

    addr |= offs.p4_off << 39;
    addr |= offs.p3_off << 30;
    addr |= offs.p2_off << 21;
    addr |= offs.p1_off << 12;

    return (void *)addr;
}

pt_ptr_t vmm_get_table(pt_off_t *offs, pt_t *p4) {
    pt_ptr_t ret;
    p4 = (pt_t *) ((uint64_t) p4 + NORMAL_VMA_OFFSET);
    //sprintf("\nP4: %lx", p4);
    // Is the p3 present?
    if (p4->ents[offs->p4_off] & VMM_PRESENT) {
        ret.p3 = (pt_t *) ((p4->ents[offs->p4_off] & ~(0xfff)) + NORMAL_VMA_OFFSET);
        //sprintf("\nBad pointer lel: %lx Pointer without offset: %lx", ret.p3, p4->ents[offs->p4_off] & ~(0xfff));
        //sprintf("\nP4: %lx", offs->p4_off);
    } else {
        size_t pmm_ret = pmm_allocate(0x1000);
        //sprintf("\nPMM ret: %lx", pmm_ret);
        ret.p3 = (pt_t *) ((pmm_ret + NORMAL_VMA_OFFSET));
        p4->ents[offs->p4_off] = ((uint64_t)ret.p3 - NORMAL_VMA_OFFSET)
            | VMM_PRESENT | VMM_WRITE;
        //sprintf("\nMath test: %lx Math test 2: %lx", (pmm_ret + NORMAL_VMA_OFFSET) + 1, pmm_ret);
        //sprintf("\nBad pointer lel: %lx Pointer without offset: %lx", ret.p3, p4->ents[offs->p4_off] & ~(0xfff));
        
        // Store the new entry in the table, as W/S/P (Writeable, supervisor, present)
        memset((uint8_t *) ret.p3, 0, 0x1000); // Clear the table
    }

    // Is the p2 present?
    if (ret.p3->ents[offs->p3_off] & VMM_PRESENT) {
        ret.p2 = (pt_t *) ((ret.p3->ents[offs->p3_off] & ~(0xfff)) + NORMAL_VMA_OFFSET);
    } else {
        ret.p2 = (pt_t *) (((size_t) pmm_allocate(0x1000)) + NORMAL_VMA_OFFSET);
        ret.p3->ents[offs->p3_off] = ((uint64_t)ret.p2 - NORMAL_VMA_OFFSET)
            | VMM_PRESENT | VMM_WRITE;
        // Store the new entry in the table, as W/S/P (Writeable, supervisor, present)
        memset((uint8_t *) ret.p2, 0, 0x1000); // Clear the table
    }

    // Is the p1 present?
    if (ret.p2->ents[offs->p2_off] & VMM_PRESENT) {
        ret.p1 = (pt_t *) ((ret.p2->ents[offs->p2_off] & ~(0xfff)) + NORMAL_VMA_OFFSET);
    } else {
        ret.p1 = (pt_t *) (((size_t) pmm_allocate(0x1000)) + NORMAL_VMA_OFFSET);
        ret.p2->ents[offs->p2_off] = ((uint64_t)ret.p1 - NORMAL_VMA_OFFSET)
            | VMM_PRESENT | VMM_WRITE;
        // Store the new entry in the table, as W/S/P (Writeable, supervisor, present)
        memset((uint8_t *) ret.p1, 0, 0x1000); // Clear the table
    }

    return ret;
}

void *virt_to_phys(void *virt) {
    pt_off_t offsets = vmm_virt_to_offs(virt);
    pt_ptr_t pointers = vmm_get_table(&offsets, (pt_t *) vmm_get_pml4t());
    pt_t *table = (pt_t *) pointers.p1;
    sprintf("\nP1: %u P2: %u P3: %u P4: %u Data: %lx", (uint32_t) offsets.p1_off, (uint32_t) offsets.p2_off, (uint32_t) offsets.p3_off, (uint32_t) offsets.p4_off, table->ents[offsets.p1_off]);
    return (void *) (table->ents[offsets.p1_off] & ~(0xfff));
}

int vmm_unmap_pages(pt_t *pml4, void *virt, size_t count) {
    pt_off_t offsets;
    pt_ptr_t table_addresses;
    int ret = 0;

    for (uint64_t i = 0; i < count; i++) {
        // Get table offset data
        offsets = vmm_virt_to_offs((char *) virt + (i * 0x1000));
        table_addresses = vmm_get_table(&offsets, pml4);
        if (table_addresses.p1) {
            // If the entry is already empty
            if (!(table_addresses.p1->ents[offsets.p1_off] & VMM_PRESENT)) {
                ret = 2;
                continue;
            }
            // Clear VMM_PRESENT
            table_addresses.p1->ents[offsets.p1_off] &= ~(VMM_PRESENT);
        } else {
            ret = 1;
            continue; // couldn't allocate a p1 for some reason or another
        }
    }

    return ret;
}

int vmm_map_pages(pt_t *pml4, void *virt, void *phys, size_t count, int perms) {
    pt_off_t offsets;
    pt_ptr_t table_addresses;
    int ret = 0;

    for (uint64_t i = 0; i < count; i++) {
        //sprintf("\nVirtual: %lx Phys: %lx", ((uint64_t) virt + (i * 0x1000)) & ~(0xfff), (uint64_t) ((((uint64_t) phys) & ~(0xfff)) + (i * 0x1000)));
        offsets = vmm_virt_to_offs((char *) (((uint64_t) virt + (i * 0x1000)) & ~(0xfff)));
        table_addresses = vmm_get_table(&offsets, pml4);
        if (table_addresses.p1) {
            if (table_addresses.p1->ents[offsets.p1_off] & VMM_PRESENT) {
                ret = 2;
                continue;
            }

            // Map the physical address to the virtual one by setting it's entry
            table_addresses.p1->ents[offsets.p1_off] = ((((uint64_t) phys) & ~(0xfff))
                + (i * 0x1000)) | VMM_PRESENT | VMM_WRITE | perms;
            //sprintf("\nMapped %lx with table %lx", table_addresses.p1->ents[offsets.p1_off], table_addresses.p1);
        } else {
            ret = 1;
            continue; // For some reason, the p1 pointer was null
        }
    }

    return ret;
}

int vmm_remap_pages(pt_t *pml4, void *virt, void *phys, size_t count, int perms) {
    pt_off_t offsets;
    pt_ptr_t table_addresses;
    int ret = 0;

    for (uint64_t i = 0; i < count; i++) {
        offsets = vmm_virt_to_offs((char *) virt + (i * 0x1000));
        table_addresses = vmm_get_table(&offsets, pml4);
        if (table_addresses.p1) {
            // Map the physical address to the virtual one by setting it's entry
            table_addresses.p1->ents[offsets.p1_off] = ((((uint64_t) phys) & ~(0xfff))
                + (i * 0x1000)) | VMM_PRESENT | VMM_WRITE | perms;
        } else {
            ret = 1;
            continue; // For some reason, the p1 pointer was null
        }
    }

    return ret;
}

int vmm_map(void *phys, void *virt, size_t count, int perms) {
    return vmm_map_pages((pt_t *) vmm_get_pml4t(), virt, phys, count, perms);
}

int vmm_remap(void *phys, void *virt, size_t count, int perms) {
    return vmm_remap_pages((pt_t *) vmm_get_pml4t(), virt, phys, count, perms);
}

int vmm_unmap(void *virt, size_t count) {
    return vmm_unmap_pages((pt_t *) vmm_get_pml4t(), virt, count);
}

int map_mem_block(uint64_t block_start, uint64_t block_len, uint64_t block_type) {
    /* TODO: Make a function that takes a block of memory and virtual maps it to
        higher half. Also make sure if it is a usable block (type 1), then
        first map enough to bitmap the block, then you can finish mappping the rest.
        This is so that if needed, the pmm can allocate blocks inside the new
        block of memory, giving it a low chance of running out. */

    /* Calculate rounded length of the block */
    int ret = 0;
    
    uint64_t block_end = block_start + block_len;
    uint64_t block_pages;

    block_end &= ~(0xfff); // Round the end address of the block down to a page
    block_start &= ~(0xfff); // Round the start address of the block down to a page
    block_len = block_end - block_start; // Length of the new, aligned block
    block_pages = block_len / 0x1000; // The length should be page aligned, so no rounding

    if (block_type == MULTIBOOT_MEMORY_AVAILABLE) {
        // Virtual map enough space for a bitmap, set the bitmap, and continue mapping
        uint64_t bitmap_space_needed = ((block_pages + 8 - 1) / 8) + 16; // Round up the bytes
        uint64_t bitmap_pages_needed = (bitmap_space_needed + 0x1000 - 1) / 0x1000; // Round up the pages

        // Map space for bitmap
        vmm_map((void *) block_start, (void *) (block_start + NORMAL_VMA_OFFSET), bitmap_pages_needed, 0);
        // Setup the bitmap
        set_bitmap((uint8_t *) (block_start + NORMAL_VMA_OFFSET), get_last_bitmap(bitmap), block_len, 0);
        // Map the rest of the block
        ret = vmm_map((void *) (block_start + bitmap_space_needed), (void *) (block_start + bitmap_space_needed + NORMAL_VMA_OFFSET), block_pages - bitmap_pages_needed, 0);
    } else {
        // Just map the whole block, no bitmap size calculations needed, since it's not usable
        ret = vmm_map((void *) block_start, (void *) (block_start + NORMAL_VMA_OFFSET), block_pages, 0);
    }
    vmm_flush_tlb();
    return ret;
}