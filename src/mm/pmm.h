#ifndef PMM_H
#define PMM_H
#include <stdint.h>
#include "multiboot.h"

#define NORMAL_VMA_OFFSET 0xFFFF800000000000
#define KERNEL_VMA_OFFSET 0xFFFFFFFF80000000

typedef void *symbol[];

extern symbol __kernel_end;
extern symbol __kernel_start;
extern symbol __kernel_code_start;
extern symbol __kernel_code_end;

void pmm_memory_setup(multiboot_info_t *mboot_dat);
void *pmm_alloc(uint64_t size);
void pmm_unalloc(void *addr, uint64_t size);

typedef struct {
    uint64_t byte;
    uint8_t bit;
} bitmap_index;


#endif