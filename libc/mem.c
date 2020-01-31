#include "mem.h"
#include <stdio.h>
#include <debug.h>
#include <libc.h>
#include <serial.h>
#include "../cpu/timer.h"
#include "../mem/pmm.h"
#include "../mem/vmm.h"

void *get_pointer(uint64_t addr) {
  volatile uintptr_t iptr = addr;
  unsigned int *ptr = (unsigned int*)iptr;
  return (void *)ptr;
}


void memcpy(uint8_t *source, uint8_t *dest, int nbytes) {
    int i;
    for (i = 0; i < nbytes; i++) {
        *(dest + i) = *(source + i);
    }
}

void memcpy32(uint32_t *source, uint32_t *dest, int ndwords) {
    int i;
    for (i = 0; i < ndwords; i++) {
        *(dest + i) = *(source + i);
    }
}

void memset(uint8_t *dest, uint8_t val, uint64_t len) {
    uint8_t *temp = (uint8_t *)dest;
    for ( ; len != 0; len--) {
        *temp++ = val;
    }
}

void memset32(uint32_t *dest, uint32_t val, uint64_t len) {
    uint32_t *temp = (uint32_t *)dest;
    for ( ; len != 0; len--) *temp++ = val;
}

void * kmalloc(uint64_t size) {
    uint64_t *size_ptr = (uint64_t *) ((uint64_t) pmm_allocate(size + 0x1000) + NORMAL_VMA_OFFSET);
    void *ret_ptr = size_ptr + 0x200; // 0x1000 / 8 because it's a 64 bit increment size
    *size_ptr = size + 0x1000;
    sprintf("\n[KMALLOC] Allocated %lu bytes at %lx", *size_ptr, ret_ptr);
    return ret_ptr;
}

void free(void * addr) {
    uint64_t *size_ptr = addr;
    size_ptr -= 0x200;
    sprintf("\n[FREE] Freeing addr %lx and %lu bytes", ((uint64_t) addr + 0x1000), *size_ptr);
    pmm_unallocate(addr, *size_ptr);
}