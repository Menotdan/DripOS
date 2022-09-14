#include "vmm.h"
#include "pmm.h"
#include "klibc/lock.h"
#include "klibc/string.h"
#include "drivers/serial.h"
#include "drivers/tty/tty.h"
#include "cpuid.h"
#include <stddef.h>

#include "proc/scheduler.h"

lock_t vmm_spinlock = {0, 0, 0, 0}; // Spinlock for the VMM
uint64_t base_kernel_cr3 = 0;

uint64_t cache_line_size = 0;
uint8_t vmm_complete = 0;

uint64_t vmm_get_base() {
    uint64_t ret;
    asm volatile("movq %%cr3, %0;" : "=r"(ret));
    return ret;
}

void vmm_set_base(uint64_t new) {
    asm volatile("movq %0, %%cr3;" ::"r"(new) : "memory");
}

void vmm_invlpg(uint64_t new) {
    asm volatile("invlpg (%0);" ::"r"(new) : "memory");
}

// broken clflush that isnt even needed
// void vmm_clflush(void *addr, uint64_t count) {
//     if (!cache_line_size) {
//         uint64_t a,b,c,d;
//         __cpuid(1, a, b, c, d);
//         cache_line_size = (b >> 8) & 0xff;

//         // UNUSED
//         (void) a;
//         (void) c;
//         (void) d;
//     }
//     uint64_t cur_addr = ((uint64_t) addr / cache_line_size) * cache_line_size; // Round down to align
//     uint64_t end_byte = (uint64_t) addr + count;
//     uint64_t real_end = ((end_byte + cache_line_size - 1) / cache_line_size) * cache_line_size; // Round up
//     uint64_t real_count = (real_end - cur_addr) / cache_line_size;

//     while (real_count) {
//         asm volatile("clflush (%0);" ::"r"(cur_addr) : "memory");
//         cur_addr += cache_line_size;
//         real_count--;
//     }
// }

pt_off_t vmm_virt_to_offs(void *virt) {
    uint64_t addr = (uint64_t)virt;

    pt_off_t off = {
        .p4_off = (addr & ((uint64_t) 0x1ff << 39)) >> 39,
        .p3_off = (addr & ((uint64_t) 0x1ff << 30)) >> 30,
        .p2_off = (addr & ((uint64_t) 0x1ff << 21)) >> 21,
        .p1_off = (addr & ((uint64_t) 0x1ff << 12)) >> 12,
    };

    return off;
}

void *vmm_offs_to_virt(pt_off_t offs) {
    uint64_t addr = 0;

    addr |= offs.p4_off << 39;
    addr |= offs.p3_off << 30;
    addr |= offs.p2_off << 21;
    addr |= offs.p1_off << 12;

    return (void *) addr;
}

uint64_t get_entry(page_table_t *cur_table, uint64_t offset) {
    return cur_table->entries[offset];
}

page_table_t *traverse_page_table(page_table_t *cur_table, uint64_t offset) {
    return GET_HIGHER_HALF(page_table_t *, (cur_table->entries[offset] & VMM_4K_PERM_MASK));
}

void *virt_to_phys(void *virt, page_table_t *p4) {
    uint64_t page4k_offset = ((uint64_t) virt) & 0xfff;
    uint64_t page2m_offset = ((uint64_t) virt) & 0x1fffff;
    pt_off_t offs = vmm_virt_to_offs(virt);

    p4 = GET_HIGHER_HALF(page_table_t *, p4);

    page_table_t *p3 = traverse_page_table(p4, offs.p4_off);
    if ((uint64_t) p3 > NORMAL_VMA_OFFSET) {
        page_table_t *p2 = traverse_page_table(p3, offs.p3_off);
        if (get_entry(p2, offs.p2_off) & VMM_HUGE) {
            return (void *) (p2->entries[offs.p2_off] & VMM_4K_PERM_MASK) + page2m_offset;
        }

        if ((uint64_t) p2 > NORMAL_VMA_OFFSET) {
            page_table_t *p1 = traverse_page_table(p2, offs.p2_off);

            if ((uint64_t) p1 > NORMAL_VMA_OFFSET) {
                if (p1->entries[offs.p1_off] & VMM_PRESENT) {
                    return (void *) (p1->entries[offs.p1_off] & VMM_4K_PERM_MASK) + page4k_offset;
                }
            }
        }
    }

    return (void *) 0xFFFFFFFFFFFFFFFF;
}

void *kernel_address(void *virt) {
    void *phys = virt_to_phys(virt, (page_table_t *) vmm_get_base());
    if ((uint64_t) phys != 0xFFFFFFFFFFFFFFFF) {
        return GET_HIGHER_HALF(void *, phys);
    }
    return (void *) 0;
}

void vmm_ensure_table(page_table_t *table, uint16_t offset) {
    if (!(table->entries[offset] & VMM_PRESENT)) {
        uint64_t new_table = (uint64_t) pmm_alloc(0x1000);
        memset(GET_HIGHER_HALF(uint8_t *, new_table), 0, 0x1000);
        table->entries[offset] = new_table | VMM_PRESENT | VMM_WRITE | VMM_USER;
    }
}

void vmm_remap_to_4k(page_table_t *p2, uint16_t offset) {
    uint64_t new_pml1 = (uint64_t) pmm_alloc(0x1000);
    uint64_t *new_pml1_virt = (void *) (new_pml1 + NORMAL_VMA_OFFSET);

    uint64_t represented_range = p2->entries[offset] & VMM_4K_PERM_MASK;
    uint64_t perms = p2->entries[offset] & 0xfff;
    memset((uint8_t *) new_pml1_virt, 0, 0x1000); // Touch the address there in case our data is living there

    uint64_t cur_pos = represented_range;
    for (uint64_t i = 0; i < 512; i++) {
        new_pml1_virt[i] = cur_pos | perms;
        cur_pos += 0x1000;
    }

    p2->entries[offset] = new_pml1 | VMM_PRESENT | VMM_WRITE | VMM_USER;
    vmm_invlpg(represented_range); // Invalidate any previous mappings
}

/* Check if an address is mapped */
uint8_t is_mapped(void *data) {
    interrupt_state_t state = interrupt_lock();
    lock(vmm_spinlock);
    uint64_t phys_addr = (uint64_t) virt_to_phys(data, (void *) vmm_get_base());

    if (phys_addr == 0xFFFFFFFFFFFFFFFF) {
        unlock(vmm_spinlock);
        interrupt_unlock(state);
        return 0;
    } else {
        unlock(vmm_spinlock);
        interrupt_unlock(state);
        return 1;
    }
}

uint8_t range_mapped(void *data, uint64_t size) {
    uint64_t cur_addr = (uint64_t) data & ~(0xfff);
    uint64_t addr_not_rounded = (uint64_t) data;
    uint64_t end_addr = (addr_not_rounded + size) & ~(0xfff);
    uint64_t rounded_size = end_addr - cur_addr;
    uint64_t pages = ((rounded_size + 0x1000 - 1) / 0x1000);

    for (uint64_t i = 0; i < pages; i++) {
        if (!is_mapped((void *) cur_addr)) {
            return 0;
        }
        cur_addr += 0x1000;
    }

    return 1;
}

/* TODO: Sync page tables for any CPUs running with the same CR3 */

/* Get a table for a set of offsets into the table */
pt_ptr_t vmm_get_table(pt_off_t *offs, page_table_t *p4) {
    pt_ptr_t ret;
    p4 = GET_HIGHER_HALF(page_table_t *, p4);

    vmm_ensure_table(p4, offs->p4_off);
    page_table_t *p3 = traverse_page_table(p4, offs->p4_off);
    ret.p3 = p3;

    vmm_ensure_table(p3, offs->p3_off);
    page_table_t *p2 = traverse_page_table(p3, offs->p3_off);
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

/* Map pages */
int vmm_map_pages(void *phys, void *virt, void *p4, uint64_t count, uint16_t perms) {
    interrupt_state_t state = interrupt_lock();
    lock(vmm_spinlock);

    int ret = 0;

    uint8_t *cur_virt = (uint8_t *) (((uint64_t) virt) & VMM_4K_PERM_MASK);
    uint64_t cur_phys = ((uint64_t) phys) & VMM_4K_PERM_MASK;

    for (uint64_t page = 0; page < count; page++) {
        pt_off_t offs = vmm_virt_to_offs((void *) cur_virt);
        pt_ptr_t ptrs = vmm_get_table(&offs, p4);

        /* Set the addresses */
        if (!(ptrs.p1->entries[offs.p1_off] & VMM_PRESENT)) {
            ptrs.p1->entries[offs.p1_off] = cur_phys | perms;
            vmm_invlpg((uint64_t) cur_virt); // Invalidate pages
            cur_phys += 0x1000;
            cur_virt += 0x1000;
        } else {
            ret = 1;
            cur_phys += 0x1000;
            cur_virt += 0x1000;
            continue;
        }

        if ((uint64_t) cur_phys == 0x49D0000) {
            sprintf("Magic address 0x49D0000 mapped by %lx, starting at %lx, for %lu pages.\n", __builtin_return_address(0), virt, count);
        }

        if ((uint64_t) cur_phys == 0x535C000) {
            sprintf("Magic address 0x535C000 mapped by %lx, starting at %lx, for %lu pages.\n", __builtin_return_address(0), virt, count);
        }
    }

    if (vmm_complete) {
        //sprintf("+mapping %lu %lu %lu\n", phys, virt, count);
    }
    unlock(vmm_spinlock);
    interrupt_unlock(state);
    return ret;
}

/* Remap pages */
int vmm_remap_pages(void *phys, void *virt, void *p4, uint64_t count, uint16_t perms) {
    interrupt_state_t state = interrupt_lock();
    lock(vmm_spinlock);

    int ret = 0;

    uint8_t *cur_virt = (uint8_t *) (((uint64_t) virt) & VMM_4K_PERM_MASK);
    uint64_t cur_phys = ((uint64_t) phys) & VMM_4K_PERM_MASK;

    if (vmm_complete) {
        //sprintf("-mapping %lu %lu %lu\n", phys, virt, count);
    }

    for (uint64_t page = 0; page < count; page++) {
        pt_off_t offs = vmm_virt_to_offs((void *) cur_virt);
        pt_ptr_t ptrs = vmm_get_table(&offs, p4);

        /* Set the addresses */
        ptrs.p1->entries[offs.p1_off] = cur_phys | (perms | VMM_PRESENT);

        vmm_invlpg((uint64_t) cur_virt); // Invalidate pages
        cur_phys += 0x1000;
        cur_virt += 0x1000;

        if ((uint64_t) cur_phys == 0x49D0000) {
            sprintf("Magic address 0x49D0000 remapped by %lx, starting at %lx, for %lu pages.\n", __builtin_return_address(0), virt, count);
        }

        if ((uint64_t) cur_phys == 0x535C000) {
            sprintf("Magic address 0x535C000 remapped by %lx, starting at %lx, for %lu pages.\n", __builtin_return_address(0), virt, count);
        }
    }
    if (vmm_complete) {
        //sprintf("+mapping %lu %lu %lu\n", phys, virt, count);
    }


    unlock(vmm_spinlock);
    interrupt_unlock(state);
    return ret;
}

/* Unmap pages */
int vmm_unmap_pages(void *virt, void *p4, uint64_t count) {
    interrupt_state_t state = interrupt_lock();
    lock(vmm_spinlock);

    int ret = 0;
    uint64_t cur_virt = (uint64_t) virt;

    if (vmm_complete) {
        //sprintf("-mapping %lu %lu %lu\n", virt_to_phys(virt, p4), virt, count);
    }
    for (uint64_t page = 0; page < count; page++) {
        pt_off_t offs = vmm_virt_to_offs((void *) cur_virt);
        pt_ptr_t ptrs = vmm_get_table(&offs, p4);

        uint64_t start = ((uint64_t) virt) & (~((uint64_t) 0xFFFFFFFF80000000));
        uint64_t end = start + (count * 0x1000);
        
        if (start <= 0x49D0000 && end > 0x49D0000) {
            sprintf("Magic address 0x49D0000 unmapped by %lx, starting at %lx, for %lu pages.\n", __builtin_return_address(0), virt, count);
        }

        if (start <= 0x535C000 && end > 0x535C000) {
            sprintf("Magic address 0x535C000 unmapped by %lx, starting at %lx, for %lu pages.\n", __builtin_return_address(0), virt, count);
        }

        /* Set the addresses */
        if ((ptrs.p1->entries[offs.p1_off] & VMM_PRESENT)) {
            ptrs.p1->entries[offs.p1_off] = 0;
        } else {
            ret = 1;
        }

        vmm_invlpg(cur_virt); // Invalidate pages
        cur_virt += 0x1000;
    }

    unlock(vmm_spinlock);
    interrupt_unlock(state);
    return ret;
}

/* Set the PAT entries */
void vmm_set_pat_pages(void *virt, void *p4, uint64_t count, uint8_t pat_entry) {
    interrupt_state_t state = interrupt_lock();
    lock(vmm_spinlock);

    uint64_t cur_virt = (uint64_t) virt;

    page_table_t *p4_offset = GET_HIGHER_HALF(page_table_t *, p4);

    for (uint64_t page = 0; page < count; page++) {
        pt_off_t offs = vmm_virt_to_offs((void *) cur_virt);
        
        if ((uint64_t) virt_to_phys(virt, p4) != 0xFFFFFFFFFFFFFFFF) {
            page_table_t *p3 = traverse_page_table(p4_offset, offs.p4_off);
            if (p3) {
                page_table_t *p2 = traverse_page_table(p3, offs.p3_off);
                if (p2) {
                    page_table_t *p1 = traverse_page_table(p2, offs.p2_off);
                    uint64_t cur_data = get_entry(p1, offs.p1_off);
                    uint8_t bit1 = (pat_entry & (1<<0)) == (1<<0);
                    uint8_t bit2 = (pat_entry & (1<<1)) == (1<<1);
                    uint8_t bit3 = (pat_entry & (1<<2)) == (1<<2);

                    cur_data |= (bit1 << 3) | (bit2 << 4) | (bit3 << 7);
                    p1->entries[offs.p1_off] = cur_data;
                }
            }
        }
        vmm_invlpg(cur_virt);
        cur_virt += 0x1000;
    }

    unlock(vmm_spinlock);
    interrupt_unlock(state);
}

void *vmm_fork_higher_half(void *old) {
    interrupt_state_t state = interrupt_lock();
    lock(vmm_spinlock);
    page_table_t *old_p4 = GET_HIGHER_HALF(page_table_t *, old);
    void *ret = pmm_alloc(0x1000);
    page_table_t *new_p4 = GET_HIGHER_HALF(page_table_t *, ret);

    memset((uint8_t *) new_p4, 0, 0x1000);

    for (uint16_t i = 256; i < 512; i++) {
        new_p4->entries[i] = old_p4->entries[i];
    }

    unlock(vmm_spinlock);
    interrupt_unlock(state);
    return ret;
}

void *vmm_fork(void *old) {
    void *ret = vmm_fork_higher_half(old);
    page_table_t *table = GET_HIGHER_HALF(page_table_t *, old);
    interrupt_state_t state = interrupt_lock();
    lock(vmm_spinlock);
    for (uint64_t w = 0; w < 256; w++) {
        /* P4 */
        if (table->entries[w] & VMM_PRESENT) {
            page_table_t *table_z = GET_HIGHER_HALF(page_table_t *, table->entries[w] & VMM_4K_PERM_MASK);
            for (uint64_t z = 0; z < 512; z++) {
                /* P3 */
                if (table_z->entries[z] & VMM_PRESENT) {
                    page_table_t *table_y = GET_HIGHER_HALF(page_table_t *, table_z->entries[z] & VMM_4K_PERM_MASK);
                    for (uint64_t y = 0; y < 512; y++) {
                        /* P2 */
                        if (table_y->entries[y] & VMM_PRESENT) {
                            page_table_t *table_x = GET_HIGHER_HALF(page_table_t *, table_y->entries[y] & VMM_4K_PERM_MASK);
                            for (uint64_t x = 0; x < 512; x++) {
                                /* P1 */
                                if (table_x->entries[x] & VMM_PRESENT) {
                                    pt_off_t offs = {w, z, y, x};
                                    void *virt = vmm_offs_to_virt(offs);
                                    void *phys = virt_to_phys(virt, GET_LOWER_HALF(page_table_t *, table));
                                    void *new_phys = pmm_alloc(0x1000);

                                    /* Copy the data */
                                    memcpy64(GET_HIGHER_HALF(void *, phys), GET_HIGHER_HALF(void *, new_phys), 0x200);

                                    /* Map new page */
                                    unlock(vmm_spinlock);
                                    vmm_map_pages(new_phys, virt, ret, 1, table_x->entries[x] & ~(VMM_4K_PERM_MASK));
                                    lock(vmm_spinlock);
                                }
                            }
                        }
                    }
                }
            }
        }
    } 
    unlock(vmm_spinlock);
    interrupt_unlock(state);
    return ret;
}

uint8_t is_mapped_in_userspace(void *phys_address) {
    page_table_t *table = GET_HIGHER_HALF(page_table_t *, vmm_get_base());
    uint8_t ret = 0;
    interrupt_state_t state = interrupt_lock();
    lock(vmm_spinlock);
    for (uint64_t w = 0; w < 256; w++) {
        /* P4 */
        if (table->entries[w] & VMM_PRESENT) {
            page_table_t *table_z = GET_HIGHER_HALF(page_table_t *, table->entries[w] & VMM_4K_PERM_MASK);
            for (uint64_t z = 0; z < 512; z++) {
                /* P3 */
                if (table_z->entries[z] & VMM_PRESENT) {
                    page_table_t *table_y = GET_HIGHER_HALF(page_table_t *, table_z->entries[z] & VMM_4K_PERM_MASK);
                    for (uint64_t y = 0; y < 512; y++) {
                        /* P2 */
                        if (table_y->entries[y] & VMM_PRESENT) {
                            page_table_t *table_x = GET_HIGHER_HALF(page_table_t *, table_y->entries[y] & VMM_4K_PERM_MASK);
                            for (uint64_t x = 0; x < 512; x++) {
                                /* P1 */
                                if (table_x->entries[x] & VMM_PRESENT) {
                                    pt_off_t offs = {w, z, y, x};
                                    void *virt = vmm_offs_to_virt(offs);
                                    void *phys = virt_to_phys(virt, GET_LOWER_HALF(page_table_t *, table));
                                    if (phys == phys_address && table_x->entries[x] & VMM_USER) {
                                        unlock(vmm_spinlock);
                                        ret = 1;
                                        return ret;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    } 
    unlock(vmm_spinlock);
    interrupt_unlock(state);
    return ret;
}

uint8_t range_mapped_in_userspace(void *data, uint64_t size) {
    uint64_t cur_addr = (uint64_t) data & ~(0xfff);
    uint64_t addr_not_rounded = (uint64_t) data;
    uint64_t end_addr = (addr_not_rounded + size) & ~(0xfff);
    uint64_t rounded_size = end_addr - cur_addr;
    uint64_t pages = ((rounded_size + 0x1000 - 1) / 0x1000);

    for (uint64_t i = 0; i < pages; i++) {
        if (!is_mapped_in_userspace((void *) cur_addr)) {
            return 0;
        }
        cur_addr += 0x1000;
    }

    return 1;
}

void vmm_deconstruct_address_space(void *old) {
    page_table_t *table = GET_HIGHER_HALF(page_table_t *, old);
    interrupt_state_t state = interrupt_lock();
    lock(vmm_spinlock);
    for (uint64_t w = 0; w < 256; w++) {
        /* P4 */
        if (table->entries[w] & VMM_PRESENT) {
            page_table_t *table_z = GET_HIGHER_HALF(page_table_t *, table->entries[w] & VMM_4K_PERM_MASK);
            for (uint64_t z = 0; z < 512; z++) {
                /* P3 */
                if (table_z->entries[z] & VMM_PRESENT) {
                    page_table_t *table_y = GET_HIGHER_HALF(page_table_t *, table_z->entries[z] & VMM_4K_PERM_MASK);
                    for (uint64_t y = 0; y < 512; y++) {
                        /* P2 */
                        if (table_y->entries[y] & VMM_PRESENT) {
                            page_table_t *table_x = GET_HIGHER_HALF(page_table_t *, table_y->entries[y] & VMM_4K_PERM_MASK);
                            for (uint64_t x = 0; x < 512; x++) {
                                /* P1 */
                                if (table_x->entries[x] & VMM_PRESENT) {
                                    pt_off_t offs = {w, z, y, x};
                                    void *virt = vmm_offs_to_virt(offs);
                                    void *phys = (void *) (table_x->entries[x] & VMM_4K_PERM_MASK);
                                    if ((uint64_t) phys == 0x49D0000) {
                                        sprintf("Magic address 0x49D0000 deconstructed by %lx, at virtual address %lx\n", __builtin_return_address(0), virt);
                                    }

                                    if ((uint64_t) phys == 0x535C000) {
                                        sprintf("Magic address 0x535C000 deconstructed by %lx, at virtual address %lx\n", __builtin_return_address(0), virt);
                                    }
                                    pmm_unalloc(phys, 0x1000);
                                }
                            }
                            pmm_unalloc(GET_LOWER_HALF(void *, table_x), 0x1000);
                        }
                    }
                    pmm_unalloc(GET_LOWER_HALF(void *, table_y), 0x1000);
                }
            }
            pmm_unalloc(GET_LOWER_HALF(void *, table_z), 0x1000);
        }
    }
    pmm_unalloc(old, 0x1000);
    unlock(vmm_spinlock);
    interrupt_unlock(state);
}

int vmm_map(void *phys, void *virt, uint64_t count, uint16_t perms) {
    return vmm_map_pages(phys, virt, (void *) vmm_get_base(), count, perms);
}

int vmm_remap(void *phys, void *virt, uint64_t count, uint16_t perms) {
    return vmm_remap_pages(phys, virt, (void *) vmm_get_base(), count, perms);
}

int vmm_unmap(void *virt, uint64_t count) {
    return vmm_unmap_pages(virt, (void *) vmm_get_base(), count);
}

void vmm_set_pat(void *virt, uint64_t count, uint8_t pat_entry) {
    vmm_set_pat_pages(virt, (void *) vmm_get_base(), count, pat_entry);
}