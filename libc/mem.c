#include "mem.h"


void memory_copy(uint8_t *source, uint8_t *dest, int nbytes) {
    int i;
    for (i = 0; i < nbytes; i++) {
        *(dest + i) = *(source + i);
    }
}

void memory_set(uint8_t *dest, uint8_t val, uint32_t len) {
    uint8_t *temp = (uint8_t *)dest;
    for ( ; len != 0; len--) *temp++ = val;
}



/* This should be computed at link time (NO) 
 * This is calculated by memory map parsing.
 */
uint32_t free_mem_addr = 0;
uint32_t memoryRemaining = 0;
uint32_t usedMem = 0;
void set_addr(uint32_t addr, uint32_t memSize) {
    free_mem_addr = addr;
    memoryRemaining = memSize;
}
/* Implementation is just a pointer to some free memory which
 * keeps growing */
uint32_t kmalloc(uint32_t size, int align, uint32_t *phys_addr) {
    /* Pages are aligned to 4K, or 0x1000 */
    if (align == 1 && (free_mem_addr & 0xFFFFF000)) {
        free_mem_addr &= 0xFFFFF000;
        free_mem_addr += 0x1000;
    }
    /* Save also the physical address */
    if (phys_addr) *phys_addr = free_mem_addr;

    uint32_t ret = free_mem_addr;
    free_mem_addr += size; /* Remember to increment the pointer */
    memoryRemaining -= size;
    usedMem += size;
    return ret;
}

void free(uint32_t address, uint32_t size) {
    memory_set(address, 0, size);
    memoryRemaining += size;
    usedMem -= size;
    free_mem_addr -= size;
}