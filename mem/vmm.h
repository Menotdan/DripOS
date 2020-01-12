#include <stdint.h>
#include <stddef.h>
#include "pmm.h"
#include "../libc/mem.h"
#define DEFAULT_PML4T 0x1000
#define VMM_ADDR_MASK ~(0xfff)
#define VMM_FLAG_PRESENT	(1<<0)
#define VMM_FLAG_WRITE		(1<<1)
#define VIRT_PHYS_BASE 0x1000000000000000

typedef struct table_address
{
    uint64_t address; // The address of the page table entry this struct references
    uint16_t pml4t_pos; // The entry in the pml4t this struct references
    uint16_t pdpt_pos; // The entry in the pdpt this struct references
    uint16_t pdt_pos; // The entry in the pdt this struct references
    uint16_t pt_pos; // The entry in the pt this struct references
    uint16_t pdpt_exists; // Does the entry in the PML4T exist?
    uint16_t pdt_exists; // Does the entry in the PDPT exist?
    uint16_t pt_exists; // Does the entry in the PDT exist?
    uint16_t pte_exists; // Does the entry in the PT exist?
} table_address_t;

typedef struct page_table
{
    uint64_t ents[512];
} pt_t;

typedef struct {
	size_t pml4_off;
	size_t pdp_off;
	size_t pd_off;
	size_t pt_off;
} pt_off_t;

typedef struct used_free12K
{
    uint8_t used_pdpt;
    uint8_t used_pdt;
    uint8_t used_pt;
} used_free12K_t;

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
void set_pml4t(uint64_t new);
void *vmm_offs_to_virt(pt_off_t offs);
int vmm_map_pages(pt_t *pml4, void *virt, void *phys, size_t count, int perms);
uint64_t get_pml4t();
pt_off_t vmm_virt_to_offs(void *virt);
