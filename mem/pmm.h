#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include "../multiboot.h"
#include "../libc/symbols.h"

#define MAPPED_DEF 0x40000000 // We have a 1 GiB of deafault mapped memory
#define KERNEL_VMA_OFFSET 0xFFFFFFFF80000000 // Kernel VMA offset
#define NORMAL_VMA_OFFSET 0xFFFF800000000000 // Normal VMA offset

extern uint64_t memory_remaining;
extern uint64_t used_mem;
extern uint64_t MAX;
extern uint64_t MIN;
extern uint8_t *bitmap;
extern symbol __kernel_end;
extern symbol __kernel_start;

typedef struct pmm_usable_list {
    uint64_t size;
    uint64_t address;
    struct pmm_usable_list *next;
} pmm_usable_list_t;

void configure_mem(multiboot_info_t *mbd);
uint64_t pmm_find_free(uint64_t size);
uint64_t pmm_allocate(uint64_t size);
uint64_t get_free_mem(); // Get remaining memory
uint64_t get_used_mem(); // Get used memory
void pmm_unallocate(void * address, uint64_t size);

void set_bitmap(uint8_t *bitmap_start, uint8_t *old_bitmap, uint64_t size_of_mem, uint64_t offset);
uint8_t *get_last_bitmap(uint8_t *bitmap_start);

#endif