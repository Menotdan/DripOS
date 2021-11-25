#ifndef KLIBC_STDLIB_H
#define KLIBC_STDLIB_H
#include <stdint.h>
#include "klibc/string.h"
#include "drivers/serial.h"
#include "io/ports.h"

#define SMALL_DELAY() port_inb(0x80); port_inb(0x80); port_inb(0x80); port_inb(0x80);
#define UNUSED(param) if (param) {}
#define stringify_param(x) #x
#define stringify(x) stringify_param(x)
#define assert(statement) do { if (!(statement)) { panic("Assert failed. File: " __FILE__ ", Line: " stringify(__LINE__) " Condition: assert(" #statement ");"); } } while (0)
#define log_debug(msg) do { sprintf("File: " __FILE__ ", Line: " stringify(__LINE__) " [ Debug ] " msg "\n"); } while (0)
#define MALLOC_SIGNATURE 0xf100f333f100f333

void *kmalloc(uint64_t size);
void kfree(void *addr);
void *krealloc(void *addr, uint64_t new_size);
void *kcalloc(uint64_t size);
void unmap_alloc(void *addr);
void remap_alloc(void *addr);
void yield();
void panic(char *msg);

//extern void *cur_pain;

#endif