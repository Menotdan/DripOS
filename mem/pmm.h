#ifndef PMM_H
#define PMM_H

#include <stdint.h>
extern uint32_t memory_remaining;
extern uint32_t used_mem;
extern uint32_t bitmap_size;
extern uint32_t free_mem_addr;
extern uint32_t MAX;
extern uint32_t MIN;
extern uint8_t *bitmap;

void set_addr(uint32_t addr, uint32_t mem_size);
uint32_t pmm_find_free(uint32_t size);
uint32_t pmm_allocate(uint32_t size);
void pmm_unallocate(void * address, uint32_t size);

#endif