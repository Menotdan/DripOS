#include <stdint.h>
#include <stddef.h>
#include "pmm.h"
#include "../libc/mem.h"
#define DEFAULT_PML4T 0x1000
#define VMM_ADDR_MASK ~(0xfff)
#define VMM_PRESENT	(1<<0)
#define VMM_WRITE		(1<<1)
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
	size_t p4_off; // PML4T
	size_t p3_off; // PDPT
	size_t p2_off; // PDT
	size_t p1_off; // PT
} pt_off_t;

typedef struct {
    pt_t *p3; // PDPT address
    pt_t *p2; // PDT address
    pt_t *p1; // PT address
} pt_ptr_t;


typedef struct used_free12K
{
    uint8_t used_pdpt;
    uint8_t used_pdt;
    uint8_t used_pt;
} used_free12K_t;

void vmm_set_pml4t(uint64_t new);
void *vmm_offs_to_virt(pt_off_t offs);
int vmm_map_pages(pt_t *pml4, void *virt, void *phys, size_t count, int perms);
int map_phys_virt(void *phys, void *virt, size_t count, int perms);
uint64_t vmm_get_pml4t();
pt_off_t vmm_virt_to_offs(void *virt);
