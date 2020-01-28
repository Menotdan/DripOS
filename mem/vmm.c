#include "vmm.h"

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
	uintptr_t addr = (uintptr_t)virt;

	pt_off_t off = {
		.p4_off =	(addr & ((size_t) 0x1ff << 39)) >> 39,
		.p3_off =	(addr & ((size_t) 0x1ff << 30)) >> 30,
		.p2_off =	(addr & ((size_t) 0x1ff << 21)) >> 21,
		.p1_off =	(addr & ((size_t) 0x1ff << 12)) >> 12,
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

pt_ptr_t vmm_get_table(pt_off_t offs, pt_t *p4) {
    pt_ptr_t ret;
    
    // Is the p3 present?
    if (p4->ents[offs.p4_off] & VMM_PRESENT) {
        ret.p3 = (pt_t *) (p4->ents[offs.p4_off] & ~(0xfff));
    } else {
        ret.p3 = (pt_t *) (((size_t) pmm_allocate(0x1000)) + NORMAL_VMA_OFFSET);
        p4->ents[offs.p4_off] = ((uint64_t)ret.p3 - NORMAL_VMA_OFFSET)
            | VMM_PRESENT | VMM_WRITE;
        // Store the new entry in the table, as W/S/P (Writeable, supervisor, present)
        memset((uint8_t *) ret.p3, 0, 0x1000); // Clear the table
    }

    // Is the p2 present?
    if (ret.p3->ents[offs.p3_off] & VMM_PRESENT) {
        ret.p2 = (pt_t *) (ret.p3->ents[offs.p3_off] & ~(0xfff));
    } else {
        ret.p2 = (pt_t *) (((size_t) pmm_allocate(0x1000)) + NORMAL_VMA_OFFSET);
        ret.p3->ents[offs.p3_off] = ((uint64_t)ret.p2 - NORMAL_VMA_OFFSET)
            | VMM_PRESENT | VMM_WRITE;
        // Store the new entry in the table, as W/S/P (Writeable, supervisor, present)
        memset((uint8_t *) ret.p2, 0, 0x1000); // Clear the table
    }

    // Is the p1 present?
    if (ret.p2->ents[offs.p2_off] & VMM_PRESENT) {
        ret.p1 = (pt_t *) (ret.p2->ents[offs.p2_off] & ~(0xfff));
    } else {
        ret.p1 = (pt_t *) (((size_t) pmm_allocate(0x1000)) + NORMAL_VMA_OFFSET);
        ret.p2->ents[offs.p2_off] = ((uint64_t)ret.p1 - NORMAL_VMA_OFFSET)
            | VMM_PRESENT | VMM_WRITE;
        // Store the new entry in the table, as W/S/P (Writeable, supervisor, present)
        memset((uint8_t *) ret.p1, 0, 0x1000); // Clear the table
    }

    return ret;
}

int vmm_unmap_pages(pt_t *pml4, void *virt, size_t count) {
    pt_off_t offsets;
    pt_ptr_t table_addresses;
    int ret = 0;

    for (uint64_t i = 0; i < count; i++) {
        // Get table offset data
        offsets = vmm_virt_to_offs((char *) virt + (i * 0x1000));
        table_addresses = vmm_get_table(offsets, pml4);
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
        offsets = vmm_virt_to_offs((char *) virt + (i * 0x1000));
        table_addresses = vmm_get_table(offsets, pml4);
        if (table_addresses.p1) {
            if (table_addresses.p1->ents[offsets.p1_off] & VMM_PRESENT) {
                ret = 2;
                continue;
            }

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

int vmm_unmap(void *virt, size_t count) {
    return vmm_unmap_pages((pt_t *) vmm_get_pml4t(), virt, count);
}