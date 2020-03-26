#ifndef KLIBC_DYNARRAY_H
#define KLIBC_DYNARRAY_H
#include <stddef.h>
#include <stdint.h>
#include "klibc/lock.h"

typedef struct {
    void *data;
    uint32_t ref_count;
    uint8_t present;
} dynarray_elem_t;

typedef struct {
    dynarray_elem_t *base;
    lock_t lock;
    int64_t array_size;
} dynarray_t;

int64_t dynarray_add(dynarray_t *dynarray, void *element, uint64_t size_of_elem);
int dynarray_remove(dynarray_t *dynarray, int64_t element);
void *dynarray_getelem(dynarray_t *dynarray, int64_t element);
void dynarray_unref(dynarray_t *dynarray, int64_t element);

#endif