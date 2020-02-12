#ifndef KLIBC_VECTOR_H
#define KLIBC_VECTOR_H
#include <stdint.h>

#define VECTOR_INIT_SIZE 4
#define PTR_SIZE 8

typedef struct {
    void **items; // An array of pointers
    uint64_t max_items;
    uint64_t items_count;
} vector_t;

void vector_init(vector_t *v);
void *vector_get(vector_t *v, uint64_t index);
void vector_resize(vector_t *v, uint64_t new_size);
void vector_add(vector_t *v, void *new_item);
void vector_delete(vector_t *v, uint64_t index);
void **vector_items(vector_t *v);
void vector_uninit(vector_t *v);

#endif