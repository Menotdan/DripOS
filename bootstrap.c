#include <stdint.h>

// The bootstrap, but in C

extern uint32_t pdpt[1024];
extern uint32_t pdpt2[1024];
extern uint32_t pdpt3[1024];
extern uint32_t pml4t[1024];
extern uint32_t paging_directory1[1024];
extern uint32_t paging_directory2[1024];
extern uint32_t paging_directory3[1024];
extern uint32_t paging_directory4[1024];

extern uint64_t __kernel_end;
extern uint64_t __kernel_start;

#define START 1
#define KRNL_VMA 0xFFFFFFFF80000000
#define VMA 0xFFFF800000000000

void paging_setup() {
    pml4t[START] = (uint32_t) pdpt;
    pml4t[510 + START] = (uint32_t) pdpt2;
    pml4t[1022 + START] = (uint32_t) pdpt3;
    pml4t[START] = (uint32_t) pdpt;
    pdpt[START] = ((uint32_t) paging_directory1 | 0x3);
    pdpt2[START] = ((uint32_t) paging_directory2 | 0x3);
    pdpt3[1020 + START] = ((uint32_t) paging_directory3 | 0x3);
    pdpt3[1022 + START] = ((uint32_t) paging_directory4 | 0x3);
    uint32_t count = 0;
    uint32_t addr = 0;

    count = 2;
    for (uint32_t index = START; index < (count * 2); index += 2) {
        paging_directory1[index] = addr | 0x83;
        addr += 0x200000;
    }

    count = 512;
    addr = __kernel_end - KRNL_VMA;
    for (uint32_t index = START; index < (count * 2); index += 2) {
        paging_directory2[index] = addr | 0x83;
        addr += 0x200000;
    }

    count = 512;
    addr = __kernel_start - KRNL_VMA;
    for (uint32_t index = START; index < (count * 2); index += 2) {
        paging_directory3[index] = addr | 0x83;
        addr += 0x200000;
    }

    count = 512;

    for (uint32_t index = START; index < (count * 2); index += 2) {
        paging_directory4[index] = addr | 0x83;
        addr += 0x200000;
    }
}