#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include "../multiboot.h"

extern uint64_t memory_remaining;
extern uint64_t used_mem;
extern uint64_t bitmap_size;
extern uint64_t free_mem_addr;
extern uint64_t MAX;
extern uint64_t MIN;
extern uint8_t *bitmap;

typedef struct pmm_usable_list {
    uint64_t size;
    uint64_t address;
    struct pmm_usable_list *next;
} pmm_usable_list_t;

void configure_mem(multiboot_info_t *mbd);
uint64_t pmm_find_free(uint64_t size);
uint64_t pmm_allocate(uint64_t size);
void pmm_unallocate(void * address, uint64_t size);

#endif