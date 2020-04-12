#ifndef KLIBC_STDLIB_H
#define KLIBC_STDLIB_H
#include <stdint.h>
#include "klibc/string.h"

#define UNUSED(param) if (param) {}
#define assert(statement) if (!(statement)) { char assert_fail_msg[200] = "Assert failed. File: "; strcat(assert_fail_msg, __FILE__); strcat(assert_fail_msg, ", Function: "); strcat(assert_fail_msg, (char *) __FUNCTION__); panic(assert_fail_msg); }

void *kmalloc(uint64_t size);
void kfree(void *addr);
void *krealloc(void *addr, uint64_t new_size);
void *kcalloc(uint64_t size);
void yield();
void panic(char *msg);

#endif