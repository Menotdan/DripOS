#include "vmm.h"
#include "pmm.h"
#include "klibc/lock.h"
#include "klibc/string.h"
#include "drivers/serial.h"
#include "drivers/tty/tty.h"
#include <stddef.h>

lock_t vmm_spinlock = 0; // Spinlock for the VMM
uint64_t base_kernel_cr3 = 0;

uint64_t vmm_get_pml4t() {
    uint64_t ret;
    asm volatile("movq %%cr3, %0;" : "=r"(ret));
    return ret;
}

void vmm_set_pml4t(uint64_t new) {
    sprintf("\nNew: %lx", new);
    asm volatile("movq %0, %%cr3;" ::"r"(new) : "memory");
}

void vmm_invlpg(uint64_t new) {
    asm volatile("invlpg (%0);" ::"r"(new) : "memory");
}

void vmm_flush_tlb() {
    vmm_set_pml4t(vmm_get_pml4t());
}

pt_off_t vmm_virt_to_offs(void *virt) {
    uintptr_t addr = (uintptr_t)virt;

    pt_off_t off = {
        .p4_off = (addr & ((size_t)0x1ff << 39)) >> 39,
        .p3_off = (addr & ((size_t)0x1ff << 30)) >> 30,
        .p2_off = (addr & ((size_t)0x1ff << 21)) >> 21,
        .p1_off = (addr & ((size_t)0x1ff << 12)) >> 12,
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

uint64_t get_entry(pt_t *cur_table, uint64_t offset) {
    return cur_table->table[offset];
}

pt_t *traverse_page_table(pt_t *cur_table, uint64_t offset) {
    return (pt_t *) ((cur_table->table[offset] & ~(0xfff)) + NORMAL_VMA_OFFSET);
}

void *virt_to_phys(void *virt, pt_t *p4) {
    uint64_t page_offset = ((uint64_t) virt) & 0xfff;
    pt_off_t offs = vmm_virt_to_offs(virt);

    p4 = (pt_t *) ((uint64_t) p4 + NORMAL_VMA_OFFSET);

    pt_t *p3 = traverse_page_table(p4, offs.p4_off);
    if ((uint64_t) p3 > NORMAL_VMA_OFFSET) {
        pt_t *p2 = traverse_page_table(p3, offs.p3_off);

        if ((uint64_t) p2 > NORMAL_VMA_OFFSET) {
            pt_t *p1 = traverse_page_table(p2, offs.p2_off);

            if ((uint64_t) p1 > NORMAL_VMA_OFFSET) {
                return (void *) ((p1->table[offs.p1_off] & ~(0xfff)) + page_offset);
            }
        }
    }

    return (void *) 0xFFFFFFFFFFFFFFFF;
}

void vmm_ensure_table(pt_t *table, uint16_t offset) {
    if (!(table->table[offset] & VMM_PRESENT)) {
        uint64_t new_table = (uint64_t) pmm_alloc(0x1000);
        memset((uint8_t *) (new_table + NORMAL_VMA_OFFSET), 0, 0x1000);
        table->table[offset] = new_table | VMM_PRESENT | VMM_WRITE | VMM_USER;
    }
}

void vmm_remap_to_4k(pt_t *p2, uint16_t offset) {
    uint64_t new_pml1 = (uint64_t) pmm_alloc(0x1000);
    uint64_t *new_pml1_virt = (void *) (new_pml1 + NORMAL_VMA_OFFSET);

    uint64_t represented_range = p2->table[offset] & ~(0x1FFFFF);
    uint64_t perms = p2->table[offset] & 0xfff;
    memset((uint8_t *) new_pml1_virt, 0, 0x1000); // Touch the address there in case our data is living there

    uint64_t cur_pos = represented_range;
    for (uint64_t i = 0; i < 512; i++) {
        new_pml1_virt[i] = cur_pos | perms;
        cur_pos += 0x1000;
    }

    p2->table[offset] = new_pml1 | VMM_PRESENT | VMM_WRITE | VMM_USER;
    vmm_invlpg(represented_range); // Invalidate any previous mappings
}

/* Check if an address is mapped */
uint8_t is_mapped(void *data) {
    lock(&vmm_spinlock);
    uint64_t phys_addr = (uint64_t) virt_to_phys(data, (void *) vmm_get_pml4t());

    if (phys_addr == 0xFFFFFFFFFFFFFFFF) {
        unlock(&vmm_spinlock);
        return 0;
    } else {
        unlock(&vmm_spinlock);
        return 1;
    }
}

/* TODO: Sync page tables for any CPUs running with the same CR3 */

/* Get a table for a set of offsets into the table */
pt_ptr_t vmm_get_table(pt_off_t *offs, pt_t *p4) {
    pt_ptr_t ret;
    p4 = (pt_t *) ((uint64_t) p4 + NORMAL_VMA_OFFSET);

    vmm_ensure_table(p4, offs->p4_off);
    pt_t *p3 = traverse_page_table(p4, offs->p4_off);
    ret.p3 = p3;

    vmm_ensure_table(p3, offs->p3_off);
    pt_t *p2 = traverse_page_table(p3, offs->p3_off);
    ret.p2 = p2;
    
    uint64_t p2_entry = get_entry(p2, offs->p2_off);
    if (!(p2_entry & VMM_HUGE) && p2_entry & VMM_PRESENT) {
        ret.p1 = traverse_page_table(p2, offs->p2_off);
        return ret;
    }

    if (p2_entry & VMM_HUGE && p2_entry & VMM_PRESENT) {
        /* Remap to 4 Kib pages */
        vmm_remap_to_4k(p2, offs->p2_off);
    }

    vmm_ensure_table(p2, offs->p2_off); // If the table didn't get remapped, ensure there is a table
    ret.p1 = traverse_page_table(p2, offs->p2_off);

    return ret;
}

int vmm_map_pages(void *phys, void *virt, void *p4, uint64_t count, uint16_t perms) {
    lock(&vmm_spinlock);

    int ret = 0;

    uint8_t *cur_virt = (uint8_t *) (((uint64_t) virt) & ~(0xfff));
    uint64_t cur_phys = ((uint64_t) phys) & ~(0xfff);

    for (uint64_t page = 0; page < count; page++) {
        pt_off_t offs = vmm_virt_to_offs((void *) cur_virt);
        pt_ptr_t ptrs = vmm_get_table(&offs, p4);

        /* Set the addresses */
        if (!(ptrs.p1->table[offs.p1_off] & VMM_PRESENT)) {
            ptrs.p1->table[offs.p1_off] = cur_phys | perms;
            cur_phys += 0x1000;
            cur_virt += 0x1000;
        } else {
            ret = 1;
            cur_phys += 0x1000;
            cur_virt += 0x1000;
            continue;
        }
        vmm_invlpg((uint64_t) cur_virt - 0x1000);
    }
    unlock(&vmm_spinlock);
    return ret;
}

int vmm_remap_pages(void *phys, void *virt, void *p4, uint64_t count, uint16_t perms) {
    lock(&vmm_spinlock);

    int ret = 0;

    uint8_t *cur_virt = (uint8_t *) (((uint64_t) virt) & ~(0xfff));
    uint64_t cur_phys = ((uint64_t) phys) & ~(0xfff);

    for (uint64_t page = 0; page < count; page++) {
        pt_off_t offs = vmm_virt_to_offs((void *) cur_virt);
        pt_ptr_t ptrs = vmm_get_table(&offs, p4);

        /* Set the addresses */
        ptrs.p1->table[offs.p1_off] = cur_phys | (perms | VMM_PRESENT);
        cur_phys += 0x1000;
        cur_virt += 0x1000;
        vmm_invlpg((uint64_t) cur_virt - 0x1000);
    }

    unlock(&vmm_spinlock);
    return ret;
}

int vmm_unmap_pages(void *virt, void *p4, uint64_t count) {
    lock(&vmm_spinlock);

    int ret = 0;

    uint64_t cur_virt = (uint64_t) virt;

    for (uint64_t page = 0; page < count; page++) {
        pt_off_t offs = vmm_virt_to_offs((void *) cur_virt);
        pt_ptr_t ptrs = vmm_get_table(&offs, p4);

        /* Set the addresses */
        if ((ptrs.p1->table[offs.p1_off] & VMM_PRESENT)) {
            ptrs.p1->table[offs.p1_off] = 0;
        } else {
            ret = 1;
        }
        cur_virt += 0x1000;
    }

    unlock(&vmm_spinlock);
    return ret;
}

void vmm_set_pat_pages(void *virt, void *p4, uint64_t count, uint8_t pat_entry) {
    lock(&vmm_spinlock);

    uint64_t cur_virt = (uint64_t) virt;

    pt_t *p4_offset = (pt_t *) ((uint64_t) p4 + NORMAL_VMA_OFFSET);

    for (uint64_t page = 0; page < count; page++) {
        pt_off_t offs = vmm_virt_to_offs((void *) cur_virt);
        
        if ((uint64_t) virt_to_phys(virt, p4) != 0xFFFFFFFFFFFFFFFF) {
            pt_t *p3 = traverse_page_table(p4_offset, offs.p4_off);
            if (p3) {
                pt_t *p2 = traverse_page_table(p3, offs.p3_off);
                if (p2) {
                    pt_t *p1 = traverse_page_table(p2, offs.p2_off);
                    uint64_t cur_data = get_entry(p1, offs.p1_off);
                    uint8_t bit1 = (pat_entry & (1<<0)) == (1<<0);
                    uint8_t bit2 = (pat_entry & (1<<1)) == (1<<1);
                    uint8_t bit3 = (pat_entry & (1<<2)) == (1<<2);

                    cur_data |= (bit1 << 3) | (bit2 << 4) | (bit3 << 7);
                    p1->table[offs.p1_off] = cur_data;
                }
            }
        }
        vmm_invlpg(cur_virt);
        cur_virt += 0x1000;
    }

    unlock(&vmm_spinlock);
}

void *vmm_fork_higher_half(void *old) {
    pt_t *old_p4 = (pt_t *) old;
    void *ret = pmm_alloc(0x1000);
    pt_t *new_p4 = (pt_t *) ((uint64_t) ret + NORMAL_VMA_OFFSET);
    
    memset((uint8_t *) new_p4, 0, 0x1000);

    for (uint16_t i = 256; i < 512; i++) {
        new_p4->table[i] = old_p4->table[i];
    }

    return ret;
}

void *vmm_fork_lower_half(void *old) {
    pt_t *old_p4 = (pt_t *) old;
    void *ret = pmm_alloc(0x1000);
    pt_t *new_p4 = (pt_t *) ((uint64_t) ret + NORMAL_VMA_OFFSET);

    memset((uint8_t *) new_p4, 0, 0x1000);

    for (uint16_t i = 0; i < 256; i++) {
        new_p4->table[i] = old_p4->table[i];
    }
    return ret;
}

void *vmm_fork(void *old) {
    pt_t *old_p4 = (pt_t *) old;
    void *ret = pmm_alloc(0x1000);
    pt_t *new_p4 = (pt_t *) ((uint64_t) ret + NORMAL_VMA_OFFSET);

    memset((uint8_t *) new_p4, 0, 0x1000);

    for (uint16_t i = 0; i < 512; i++) {
        new_p4->table[i] = old_p4->table[i];
    }
    return ret;
}

int vmm_map(void *phys, void *virt, uint64_t count, uint16_t perms) {
    return vmm_map_pages(phys, virt, (void *) vmm_get_pml4t(), count, perms);
}

int vmm_remap(void *phys, void *virt, uint64_t count, uint16_t perms) {
    return vmm_remap_pages(phys, virt, (void *) vmm_get_pml4t(), count, perms);
}

int vmm_unmap(void *virt, uint64_t count) {
    return vmm_unmap_pages(virt, (void *) vmm_get_pml4t(), count);
}

void vmm_set_pat(void *virt, uint64_t count, uint8_t pat_entry) {
    vmm_set_pat_pages(virt, (void *) vmm_get_pml4t(), count, pat_entry);
}