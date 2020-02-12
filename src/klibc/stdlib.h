#ifndef KLIBC_STDLIB_H
#define KLIBC_STDLIB_H
#include <stdint.h>

void *kmalloc(uint64_t size);
void kfree(void *addr);
void *krealloc(void *addr, uint64_t new_size);

#endif