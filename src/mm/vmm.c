#include "vmm.h"
#include "pmm.h"
#include "klibc/lock.h"
#include "klibc/string.h"
#include "drivers/serial.h"
#include <stddef.h>

lock_t vmm_spinlock = 0; // Spinlock for the VMM

uint64_t vmm_get_pml4t() {
    uint64_t ret;
    asm volatile("movq %%cr3, %0;" : "=r"(ret));
    return ret;
}

void vmm_set_pml4t(uint64_t new) {
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

void *virt_to_phys(void *virt, pt_t *p4) {
    pt_off_t offs = vmm_virt_to_offs(virt);

    if (!(p4->table[offs.p4_off] & VMM_PRESENT)) {
        return (void *) 0xFFFFFFFFFFFFFFFF;
    }
    pt_t *p3 = (pt_t *) (p4->table[offs.p4_off] & ~(0xfff));

    if (!(p3->table[offs.p4_off] & VMM_PRESENT)) {
        return (void *) 0xFFFFFFFFFFFFFFFF;
    }
    pt_t *p2 = (pt_t *) (p4->table[offs.p3_off] & ~(0xfff));

    if (!(p2->table[offs.p4_off] & VMM_PRESENT)) {
        return (void *) 0xFFFFFFFFFFFFFFFF;
    }
    pt_t *p1 = (pt_t *) (p2->table[offs.p2_off] & ~(0xfff));
    
    if (!(p1->table[offs.p4_off] & VMM_PRESENT)) {
        return (void *) 0xFFFFFFFFFFFFFFFF;
    }
    return (void *) (p1->table[offs.p1_off] & ~(0xfff));
}

pt_ptr_t vmm_get_table(pt_off_t *offs, pt_t *p4, uint16_t perms) {
    pt_ptr_t ret;
    p4 = (pt_t *)((uint64_t)p4 + NORMAL_VMA_OFFSET);
    // Is the p3 present?
    if (p4->table[offs->p4_off] & VMM_PRESENT) {
        ret.p3 = (pt_t *)((p4->table[offs->p4_off] & ~(0xfff)) + NORMAL_VMA_OFFSET);
    } else {
        size_t pmm_ret = (size_t)pmm_alloc(0x1000);
        ret.p3 = (pt_t *) ((pmm_ret + NORMAL_VMA_OFFSET));
        p4->table[offs->p4_off] = ((uint64_t)ret.p3 - NORMAL_VMA_OFFSET) | perms;
        // Store the new entry in the table, as W/S/P (Writeable, supervisor, present)
        memset((uint8_t *)ret.p3, 0, 0x1000); // Clear the table
        vmm_invlpg((uint64_t)vmm_offs_to_virt(*offs));
    }

    // Is the p2 present?
    if (ret.p3->table[offs->p3_off] & VMM_PRESENT) {
        ret.p2 = (pt_t *)((ret.p3->table[offs->p3_off] & ~(0xfff)) + NORMAL_VMA_OFFSET);
    } else {
        ret.p2 = (pt_t *)(((size_t)pmm_alloc(0x1000)) + NORMAL_VMA_OFFSET);
        ret.p3->table[offs->p3_off] = ((uint64_t)ret.p2 - NORMAL_VMA_OFFSET) | perms;
        // Store the new entry in the table, as W/S/P (Writeable, supervisor, present)
        memset((uint8_t *)ret.p2, 0, 0x1000); // Clear the table
        vmm_invlpg((uint64_t)vmm_offs_to_virt(*offs));
    }

    // Is the p1 present and not huge?
    if ((ret.p2->table[offs->p2_off] & VMM_PRESENT) && !((ret.p2->table[offs->p2_off] & VMM_HUGE))) {
        ret.p1 = (pt_t *)((ret.p2->table[offs->p2_off] & ~(0xfff)) + NORMAL_VMA_OFFSET);
    } else {
        uint64_t old_p2 = ret.p2->table[offs->p2_off] & ~(0xfff);
        old_p2 &= ~(VMM_HUGE); // Remove the huge flag
        uint8_t is_huge = 0;

        if ((ret.p2->table[offs->p2_off] & VMM_HUGE)) {
            is_huge = 1;
        }

        ret.p1 = (pt_t *)(((size_t)pmm_alloc(0x1000)) + NORMAL_VMA_OFFSET);
        memset((uint8_t *)ret.p1, 0, 0x1000); // Clear the table
        // Map the old mappings but in 4 KiB if the table is huge, and invalidate the old
        // table.
        if (is_huge) {
            for (uint16_t i = 0; i < 512; i++) {
                ret.p1->table[i] = (old_p2 + (i * 0x1000)) |
                    (ret.p2->table[offs->p2_off] & 0xfff & ~(VMM_HUGE));
                vmm_invlpg(((uint64_t)vmm_offs_to_virt(*offs)) + (i * 0x1000));
            }
        }
        // Store the new entry in the table
        ret.p2->table[offs->p2_off] = ((uint64_t)ret.p1 - NORMAL_VMA_OFFSET) | perms;
    }

    return ret;
}

int vmm_map_pages(void *phys, void *virt, void *p4, uint64_t count, uint16_t perms) {
    spinlock_lock(&vmm_spinlock);

    int ret = 0;

    uint8_t *cur_virt = (uint8_t *) (((uint64_t) virt) & ~(0xfff));
    uint64_t cur_phys = ((uint64_t) phys) & ~(0xfff);

    sprintf("\nNew mapping: Virt: %lx Phys: %lx\nPage count: %lu Perms: %x", cur_virt, cur_phys, count, (uint32_t) perms);

    for (uint64_t page = 0; page < count; page++) {
        pt_off_t offs = vmm_virt_to_offs((void *) cur_virt);
        pt_ptr_t ptrs = vmm_get_table(&offs, p4, perms);

        /* Set the addresses */
        if (!(ptrs.p1->table[offs.p1_off] & VMM_PRESENT)) {
            ptrs.p1->table[offs.p1_off] = cur_phys | (perms);
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
    spinlock_unlock(&vmm_spinlock);
    return ret;
}

int vmm_remap_pages(void *phys, void *virt, void *p4, uint64_t count, uint16_t perms) {
    spinlock_lock(&vmm_spinlock);

    int ret = 0;

    uint8_t *cur_virt = (uint8_t *) (((uint64_t) virt) & ~(0xfff));
    uint64_t cur_phys = ((uint64_t) phys) & ~(0xfff);

    sprintf("\nRemap: Virt: %lx Phys: %lx\nPage count: %lu Perms: %x", cur_virt, cur_phys, count, (uint32_t) perms);

    for (uint64_t page = 0; page < count; page++) {
        pt_off_t offs = vmm_virt_to_offs((void *) cur_virt);
        pt_ptr_t ptrs = vmm_get_table(&offs, p4, perms);

        /* Set the addresses */
        ptrs.p1->table[offs.p1_off] = cur_phys | (perms | VMM_PRESENT);
        cur_phys += 0x1000;
        cur_virt += 0x1000;
        vmm_invlpg((uint64_t) cur_virt - 0x1000);
    }

    spinlock_unlock(&vmm_spinlock);
    return ret;
}

int vmm_unmap_pages(void *phys, void *virt, void *p4, uint64_t count) {
    spinlock_lock(&vmm_spinlock);

    int ret = 0;

    uint8_t *cur_virt = (uint8_t *) virt;
    uint64_t cur_phys = ((uint64_t) phys) & ~(0xfff);

    for (uint64_t page = 0; page < count; page++) {
        pt_off_t offs = vmm_virt_to_offs((void *) cur_virt);
        pt_ptr_t ptrs = vmm_get_table(&offs, p4, 0);

        /* Set the addresses */
        if ((ptrs.p1->table[offs.p1_off] & VMM_PRESENT)) {
            ptrs.p1->table[offs.p1_off] = 0;
            cur_phys += 0x1000;
            cur_virt += 0x1000;
        } else {
            ret = 1;
            cur_phys += 0x1000;
            cur_virt += 0x1000;
            continue;
        }
    }

    spinlock_unlock(&vmm_spinlock);
    return ret;
}

int vmm_map(void *phys, void *virt, uint64_t count, uint16_t perms) {
    return vmm_map_pages(phys, virt, (void *) vmm_get_pml4t(), count, perms);
}

int vmm_remap(void *phys, void *virt, uint64_t count, uint16_t perms) {
    return vmm_remap_pages(phys, virt, (void *) vmm_get_pml4t(), count, perms);
}

int vmm_unmap(void *phys, void *virt, uint64_t count) {
    return vmm_unmap_pages(phys, virt, (void *) vmm_get_pml4t(), count);
}