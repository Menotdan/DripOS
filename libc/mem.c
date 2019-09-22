#include <stdio.h>
#include <libc.h>
#include <serial.h>
#include "mem.h"

void *get_pointer(uint32_t addr) {
  //sprintd("Getting pointer to address: ");
  //sprint_uint(addr);
  //sprint("\n");
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
    sprint("\n Setting memory variables... \n  ");
    sprint_uint(addr);
    sprint("\n   ");
    sprint_uint(memSize);
    sprint("\n");
    void * t = get_pointer(free_mem_addr);
    memory_set32(t, 0, 8);
}

/* Recursive function to find the best fitting block of mem to use */
void * bestFit(uint32_t size, uint32_t curFit, uint32_t curAddr, uint32_t curFitBlock) {
    uint32_t *nextFreeBlock = get_pointer(curAddr);
    uint32_t *freeSize = get_pointer(curAddr+4);
    uint32_t fit = curFit;
    uint32_t block = curFitBlock;
    uint32_t s = size;
    if (*nextFreeBlock != 0 && *freeSize != 0 && *nextFreeBlock >= MIN && *nextFreeBlock + s <= MAX) {
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
            return get_pointer(curFitBlock);
        }
        
    } else
    {
        return get_pointer(curFitBlock);
    }
    
}

void block_move(blockData_t *d) {
    uint32_t current = d->chain_next;
    uint32_t *current_ptr = get_pointer(current);
    uint32_t *current_ptr_offset = get_pointer(current+4);
    uint32_t usedMemBlock = d->usedBlock;
    uint32_t usedBlockSize = d->usedBlockSize;

    if (*current_ptr != 0 && *current_ptr_offset != 0 && *current_ptr >= MIN && (*current_ptr + *current_ptr_offset) <= MAX) {
        // New pointer exists
        d->chain_next = *current_ptr;
        d->next_block_size = *current_ptr_offset;
        // After setting the values for the next call, handle this one
        if (*current_ptr == usedMemBlock) {
            // The next block is the block that is about to be used
            uint32_t *used_ptr = get_pointer(usedMemBlock);
            uint32_t *used_ptr_offset = get_pointer(usedMemBlock + 4);
            uint32_t used_pointing = *used_ptr;
            uint32_t used_pointing_size = *used_ptr_offset;
            *current_ptr = used_pointing;
            *current_ptr_offset = used_pointing_size;
            return;
        } else
        {
            return block_move(d);
        }
        
    } else {
        return;
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
    void * bFit = bestFit(size, MAX - free_mem_addr, free_mem_addr, free_mem_addr);
    uint32_t ret = bFit;
    blockData_t *param;
    uint32_t *f1 = get_pointer(free_mem_addr);
    uint32_t *f2 = get_pointer(free_mem_addr+4);
    param->chain_next = f1;
    param->next_block_size = f2;
    param->usedBlock = ret;
    param->usedBlockSize = size;
    block_move(param);
    if (ret == free_mem_addr) {
        free_mem_addr += size; /* Remember to increment the pointer */
    }
    memoryRemaining -= size;
    usedMem += size;
    memory_set(ret, 0, size);
    return ret;
}

void * kmalloc(uint32_t size) {
    void * t = get_pointer(kmalloc_int(size, 0));
    return t;
}

void free(void * addr, uint32_t size) {
    void *address = get_pointer(addr);
    uint32_t *free_ptr = get_pointer(free_mem_addr);
    uint32_t *free_ptr_offset = get_pointer(free_mem_addr + 4);
    uint32_t curAddr = *free_ptr;
    uint32_t curSize = *free_ptr_offset;
    uint32_t *addr_base = get_pointer(address);
    uint32_t *addr_size = get_pointer(address+4);

    if (address + size == free_mem_addr) {
        /* Add new block to the chain */
        memory_set(address, 0, size);
        uint32_t lastAddr = *free_ptr;
        uint32_t lastSize = *free_ptr_offset;
        free_mem_addr -= size;
        free_ptr = get_pointer(free_mem_addr);
        free_ptr_offset = get_pointer(free_mem_addr + 4);
        *free_ptr = lastAddr;
        *free_ptr_offset = lastSize;
    } else {
        memory_set(address, 0, size);
        *addr_base = curAddr;
        *addr_size = curSize;
        *free_ptr = address;
        *free_ptr_offset = size;
    }

    memoryRemaining += size;
    usedMem -= size;

}