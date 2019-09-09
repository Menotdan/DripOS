#ifndef MEM_H
#define MEM_H

#include "../cpu/types.h"

void memory_copy(uint8_t *source, uint8_t *dest, int nbytes);
void memory_set(uint8_t *dest, uint8_t val, uint32_t len);

/* kmalloc */
void * kmalloc(uint32_t size);

/* Setup memory address */
void set_addr(uint32_t addr, uint32_t memSize);
uint32_t memoryRemaining;
uint32_t usedMem;

/* Free some memory */
void free(void * address, uint32_t size);

/* Get a pointer from an address */
void *get_pointer(addr);

#endif
