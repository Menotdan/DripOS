#include "vmm.h"

uint64_t current_pml4t = 0;

uint64_t get_pml4t() {
    uint64_t ret;
    asm volatile("movq %%cr3, %0;":"=r"(ret));
    return ret;
}

void set_pml4t(uint64_t new) {
    asm volatile("movq %0, %%cr3;"::"r"(new) : "memory");
}
// qookie's fancy paging code
pt_off_t vmm_virt_to_offs(void *virt) {
	uintptr_t addr = (uintptr_t)virt;

	pt_off_t off = {
		.pml4_off =	(addr & ((size_t)0x1ff << 39)) >> 39,
		.pdp_off =	(addr & ((size_t)0x1ff << 30)) >> 30,
		.pd_off =	(addr & ((size_t)0x1ff << 21)) >> 21,
		.pt_off =	(addr & ((size_t)0x1ff << 12)) >> 12,
	};

	return off;
}

void *vmm_offs_to_virt(pt_off_t offs) {
	uintptr_t addr = 0;

	addr |= offs.pml4_off << 39;
	addr |= offs.pdp_off << 30;
	addr |= offs.pd_off << 21;
	addr |= offs.pt_off << 12;

	return (void *)addr;
}

static inline pt_t *vmm_get_or_alloc_ent(pt_t *tab, size_t off, int flags) {
    uint64_t ent_addr = tab->ents[off] & VMM_ADDR_MASK;
    if (!ent_addr) {
        ent_addr = tab->ents[off] = pmm_allocate(4096);
        if (!ent_addr) {
            return NULL;
        }
        tab->ents[off] |= flags | VMM_FLAG_PRESENT;
        memset((void *)(ent_addr + VIRT_PHYS_BASE), 0, 4096);
    }

    return (pt_t *)(ent_addr + VIRT_PHYS_BASE);
}

int vmm_map_pages(pt_t *pml4, void *virt, void *phys, size_t count, int perms) {
    int higher_perms = perms & (VMM_FLAG_WRITE);

    while (count--) {
        pt_off_t offs = vmm_virt_to_offs(virt);

        pt_t *pml4_virt = (pt_t *)((uint64_t)pml4 + VIRT_PHYS_BASE);
        pt_t *pdp_virt = vmm_get_or_alloc_ent(pml4_virt, offs.pml4_off, higher_perms);
        pt_t *pd_virt = vmm_get_or_alloc_ent(pdp_virt, offs.pdp_off, higher_perms);
        pt_t *pt_virt = vmm_get_or_alloc_ent(pd_virt, offs.pd_off, higher_perms);
        pt_virt->ents[offs.pt_off] = (uint64_t)phys | perms | VMM_FLAG_PRESENT;

        virt = (void *)((uintptr_t)virt + 0x1000);
        phys = (void *)((uintptr_t)phys + 0x1000);
    }

    return 1;
} 

int map_phys_virt(void *phys, void *virt, size_t count, int perms) {
    return vmm_map_pages((pt_t *)get_pml4t(), virt, phys, count, perms);
}

// table_address_t table_address_from_pos(table_address_t table_pos) {
//     page_table_t *data;
//     table_address_t ret = table_pos;
//     data = (page_table_t *)current_pml4t;
//     uint64_t data_temp;
//     data_temp = data->data[table_pos.pml4t_pos];
//     ret.pdpt_exists = data_temp & 0xfff;
//     if (!(ret.pdpt_exists)) {
//         ret.pdt_exists = 0;
//         ret.pt_exists = 0;
//         ret.pte_exists = 0;
//         ret.address = 0;
//         return ret;
//     }

//     data = (page_table_t *)(data_temp & (~0xfff));
//     data_temp = data->data[table_pos.pdpt_pos];
//     ret.pdt_exists = data_temp & 0xfff;
//     if (!(ret.pdt_exists)) {
//         ret.pt_exists = 0;
//         ret.pte_exists = 0;
//         ret.address = 0;
//         return ret;
//     }

//     data = (page_table_t *)(data_temp & (~0xfff));
//     data_temp = data->data[table_pos.pdt_pos];
//     ret.pt_exists = data_temp & 0xfff;
//     if (!(ret.pt_exists)) {
//         ret.pte_exists = 0;
//         ret.address = 0;
//         return ret;
//     }

//     data = (page_table_t *)(data_temp & (~0xfff));
//     data_temp = data->data[table_pos.pt_pos];
//     ret.pte_exists = data_temp & 0xfff;

//     ret.address = ((uint64_t)data) + (table_pos.pt_pos * 8);
//     return ret;
// }

// table_address_t address_to_table_pos(uint64_t address) {
//     // Convert a virtual address into a table position
//     table_address_t ret;
//     uint64_t page_addr = address & ~(0xfff);
//     ret.pml4t_pos = (page_addr & ((uint64_t)0x1ff << 39)) >> 39;
//     ret.pdpt_pos = (page_addr & ((uint64_t)0x1ff << 30)) >> 30;
//     ret.pdt_pos = (page_addr & ((uint64_t)0x1ff << 21)) >> 21;
//     ret.pt_pos = (page_addr & ((uint64_t)0x1ff << 12)) >> 12;
//     ret = table_address_from_pos(ret);
    
//     return ret;
// }

// uint64_t map_virtual_physical(uint64_t free12K, uint64_t physical, uint64_t virtual) {
//     uint64_t amount_used;
//     table_address_t table_pos = address_to_table_pos(virtual);

//     if (table_pos.address == 0) {
//         // The table space we wanted is not filled
//         if (!table_pos.pte_exists) {

//         }
//     }
// }

// void setup_paging(uint64_t size_of_free_space, uint64_t free_space, uint64_t framebuffer_address, uint64_t framebuffer_size) {     
//     current_pml4t = DEFAULT_PML4T;
// }