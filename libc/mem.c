#include <stdio.h>
#include <libc.h>
#include "mem.h"

void *get_pointer(uint32_t addr) {
  kprint("\nGetting pointer to address: ");
  kprint_uint(addr);
  volatile uintptr_t iptr = addr;
  unsigned int *ptr = (unsigned int*)iptr;
  /* ... */
  
  kprint("\nPointer address: ");
  kprint_uint(&ptr);
  kprint("\nIPointer address: ");
  kprint_uint(iptr);
  return (void *)ptr;
}

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

void memory_set32(uint32_t *dest, uint32_t val, uint32_t len) {
    uint32_t *temp = (uint32_t *)dest;
    for ( ; len != 0; len--) *temp++ = val;
}

/* 
 * This is calculated by memory map parsing.
 */
uint32_t new_addr = 0;
uint32_t prev_addr = 0;
uint32_t mem_size = 0;
uint32_t free_mem_addr = 0;
//volatile uintptr_t iptr;
uint32_t memoryRemaining = 0;
uint32_t usedMem = 0;
void set_addr(uint32_t addr, uint32_t memSize) {
    free_mem_addr = addr;
    memoryRemaining = memSize;
}
/* Implementation is just an address which
 * keeps growing */
uint32_t kmalloc_int(uint32_t size, int align) {
    /* Pages are aligned to 4K, or 0x1000 */
    if (align == 1 && (free_mem_addr & 0xFFFFF000)) {
        free_mem_addr &= 0xFFFFF000;
        free_mem_addr += 0x1000;
    }
    /* Save also the physical address */
    //if (phys_addr) *phys_addr = free_mem_addr;
    //uint32_t *address_ptr = get_pointer(free_mem_addr);
    //memory_set32(address_ptr, 0, size);
    uint32_t ret = free_mem_addr;
    free_mem_addr += size; /* Remember to increment the pointer */
    memoryRemaining -= size;
    usedMem += size;
    return ret;
}

void * kmalloc(uint32_t size) {
    return (void*)get_pointer(kmalloc_int(size, 0));
}

void free(void * address, uint32_t size) {
    uint32_t *free_ptr = get_pointer(free_mem_addr);
    uint32_t *free_ptr_offset = get_pointer(free_mem_addr + 4);
    kprint("Address param: ");
    kprint_uint(&address);
    memory_set32(address, 0, size);
    memory_set32(free_ptr, &address, 1);
    memory_set32(free_ptr_offset, size, 1);
    kprint("\nAddress of new free mem: ");
    char free_mem_addr_tmp[20];
    hex_to_ascii(*free_ptr, free_mem_addr_tmp);
    kprint(free_mem_addr_tmp);
    kprint("\nAddress of new free mem, but not hex: ");
    kprint_uint(*free_ptr);
    kprint("\nCurrent free mem pointer: ");
    kprint_uint(free_mem_addr);
    kprint("\nSize: ");
    kprint_uint(*free_ptr_offset);
    kprint("\nSize, but from the param: ");
    kprint_uint(size);
    memoryRemaining += size;
    usedMem -= size;
    free_mem_addr -= size;
}