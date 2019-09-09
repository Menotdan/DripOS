#include <stdio.h>
#include <libc.h>
#include "mem.h"

void *get_pointer(uint32_t addr) {
  //kprint("\nGetting pointer to address: ");
  //kprint_uint(addr);
  volatile uintptr_t iptr = addr;
  unsigned int *ptr = (unsigned int*)iptr;
  /* ... */
  
  /*kprint("\nPointer address: ");
  kprint_uint(&ptr);
  kprint("\nIPointer address: ");
  kprint_uint(iptr);
  kprint("\nAddress pointed to: ");
  kprint_uint(ptr);*/
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
uint32_t MAX = 0;
uint32_t MIN = 0;
void set_addr(uint32_t addr, uint32_t memSize) {
    free_mem_addr = addr;
    memoryRemaining = memSize;
    MAX = mem_size + addr;
    MIN = free_mem_addr;
    void * t = get_pointer(free_mem_addr);
    //memory_set32(t, 0, 0x1000);
}

/* Recursive function to find the best fitting block of mem to use */
void * bestFit(uint32_t size, uint32_t curFit, uint32_t curAddr, uint32_t curFitBlock) {
    uint32_t *nextFreeBlock = get_pointer(curAddr);
    uint32_t *freeSize = get_pointer(curAddr+4);
    uint32_t fit = curFit;
    uint32_t block = curFitBlock;
    uint32_t s = size;
    if (*nextFreeBlock != 0 && *freeSize != 0 && *nextFreeBlock >= MIN && *nextFreeBlock <= MAX) {
        /* There is actually memory here */
        uint32_t data = 180; // Random test value
        uint32_t *ptr = get_pointer(*nextFreeBlock+8);
        *ptr = data;
        uint32_t inputD = *ptr;
        if (inputD == data) {
            /* Cool, the memory works */
            //kprint("Memory works!");
            if (size <= *freeSize) {
                uint32_t dif = abs(*freeSize-size);
                if (dif < curFit) {
                    fit = dif;
                    block = *nextFreeBlock;
                }
            }
            return bestFit(s, fit, *nextFreeBlock, block);
        } else
        {
            //kprint("Memory not valid...");
            return get_pointer(curFitBlock);
        }
        
    } else
    {
        return get_pointer(curFitBlock);
    }
    
}

/* Implementation is just an address which
 * keeps growing, and a chunk scanner to find free chunks. */
uint32_t kmalloc_int(uint32_t size, int align) {
    /* Pages are aligned to 4K, or 0x1000 */
    if (align == 1 && (free_mem_addr & 0xFFFFF000)) {
        free_mem_addr &= 0xFFFFF000;
        free_mem_addr += 0x1000;
    }
    /* Save also the physical address */
    //if (phys_addr) *phys_addr = free_mem_addr;
    //uint32_t *address_ptr = get_pointer(free_mem_addr);
    //memory_set32(get_pointer(free_mem_addr), 0, size);
    void * bFit = bestFit(size, memoryRemaining, free_mem_addr, free_mem_addr);
    uint32_t ret = bFit;
    kprint("\nBest fitting mem as determined: ");
    kprint_uint(ret);
    free_mem_addr += size; /* Remember to increment the pointer */
    memoryRemaining -= size;
    usedMem += size;
    return ret;
}

void * kmalloc(uint32_t size) {
    void * t = get_pointer(kmalloc_int(size, 0));
    //kprint_uint(t);
    return t;
}

void free(void * address, uint32_t size) {
    uint32_t *free_ptr = get_pointer(free_mem_addr);
    uint32_t *free_ptr_offset = get_pointer(free_mem_addr + 4);
    uint32_t curAddr = *free_ptr;
    uint32_t curSize = *free_ptr_offset;
    uint32_t *addr_base = get_pointer(address);
    uint32_t *addr_size = get_pointer(address+4);
    kprint("Address param: ");
    kprint_uint(address);
    memory_set32(address, 0, size);
    *addr_base = curAddr;
    *addr_size = curSize;
    memory_set32(free_ptr, address, 1);
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