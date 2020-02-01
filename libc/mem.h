#ifndef MEM_H
#define MEM_H

#include "../cpu/types.h"

void memcpy32(uint32_t *source, uint32_t *dest, int ndwords);
void memcpy(uint8_t *source, uint8_t *dest, int nbytes);
void memset(uint8_t *dest, uint8_t val, uint64_t len);
void memset32(uint32_t *dest, uint32_t val, uint64_t len);

/* kmalloc */
void * kmalloc(uint64_t size);

uint32_t kmalloc_bitmap(uint32_t size);

/* Free some memory */
void free(void * address);

/* Get a pointer from an address */
void *get_pointer(uint64_t addr);

typedef struct block_move_data
{
    uint32_t chain_next;
    uint32_t next_block_size;
    uint32_t usedBlock;
    uint32_t usedBlockSize;
}__attribute__((packed)) blockData_t;


#endif
