#ifndef KLIBC_STDLIB_H
#define KLIBC_STDLIB_H
#include <stdint.h>
#include "klibc/string.h"
#include "drivers/serial.h"

#define UNUSED(param) if (param) {}
#define stringify_param(x) #x
#define stringify(x) stringify_param(x)
#define assert(statement) do { if (!(statement)) { panic("Assert failed. File: " __FILE__ ", Line: " stringify(__LINE__)); } } while (0)
#define log_debug(msg) do { sprintf("\nFile: " __FILE__ ", Line: " stringify(__LINE__) " [ Debug ] " msg); } while (0)

void *kmalloc(uint64_t size);
void kfree(void *addr);
void *krealloc(void *addr, uint64_t new_size);
void *kcalloc(uint64_t size);
void yield();
void panic(char *msg);

#endif