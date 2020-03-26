#ifndef KLIBC_STDLIB_H
#define KLIBC_STDLIB_H
#include <stdint.h>

#define UNUSED(param) if (param) {}

void *kmalloc(uint64_t size);
void kfree(void *addr);
void *krealloc(void *addr, uint64_t new_size);
void *kcalloc(uint64_t size);
void yield();
void panic();

#endif