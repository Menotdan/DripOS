#include <stdint.h>
#define DEFAULT_PML4T 0x1000

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
    uint64_t data[512];
} page_table_t;

typedef struct used_free12K
{
    uint8_t used_pdpt;
    uint8_t used_pdt;
    uint8_t used_pt;
} used_free12K_t;
