#include "vmm.h"

uint64_t current_pml4t = 0;

table_address_t table_address_from_pos(table_address_t table_pos) {
    page_table_t *data;
    table_address_t ret = table_pos;
    data = (page_table_t *)current_pml4t;
    uint64_t data_temp;
    data_temp = data->data[table_pos.pml4t_pos];
    ret.pdpt_exists = data_temp & 0xfff;
    if (!(ret.pdpt_exists)) {
        ret.pdt_exists = 0;
        ret.pt_exists = 0;
        ret.pte_exists = 0;
        ret.address = 0;
        return ret;
    }

    data = (page_table_t *)(data_temp & (~0xfff));
    data_temp = data->data[table_pos.pdpt_pos];
    ret.pdt_exists = data_temp & 0xfff;
    if (!(ret.pdt_exists)) {
        ret.pt_exists = 0;
        ret.pte_exists = 0;
        ret.address = 0;
        return ret;
    }

    data = (page_table_t *)(data_temp & (~0xfff));
    data_temp = data->data[table_pos.pdt_pos];
    ret.pt_exists = data_temp & 0xfff;
    if (!(ret.pt_exists)) {
        ret.pte_exists = 0;
        ret.address = 0;
        return ret;
    }

    data = (page_table_t *)(data_temp & (~0xfff));
    data_temp = data->data[table_pos.pt_pos];
    ret.pte_exists = data_temp & 0xfff;

    ret.address = ((uint64_t)data) + (table_pos.pt_pos * 8);
    return ret;
}

table_address_t address_to_table_pos(uint64_t address) {
    // Convert a virtual address into a table position
    table_address_t ret;
    uint64_t page_addr = address & ~(0xfff);
    ret.pml4t_pos = (page_addr & ((uint64_t)0x1ff << 39)) >> 39;
    ret.pdpt_pos = (page_addr & ((uint64_t)0x1ff << 30)) >> 30;
    ret.pdt_pos = (page_addr & ((uint64_t)0x1ff << 21)) >> 21;
    ret.pt_pos = (page_addr & ((uint64_t)0x1ff << 12)) >> 12;
    ret = table_address_from_pos(ret);
    
    return ret;
}

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