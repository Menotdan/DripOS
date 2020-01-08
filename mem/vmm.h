#include <stdint.h>
typedef struct table_address
{
    uint64_t address; // The address of the page table entry this struct references
    uint16_t pml4t_pos; // The entry in the pml4t this struct references
    uint16_t pdpt_pos; // The entry in the pdpt this struct references
    uint16_t pdt_pos; // The entry in the pdt this struct references
    uint16_t pt_pos; // The entry in the pt this struct references
} table_address_t;
