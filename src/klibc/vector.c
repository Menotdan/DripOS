#include "vector.h"
#include "stdlib.h"

void vector_init(vector_t *v) {
    v->max_items = VECTOR_INIT_SIZE;
    v->items = kmalloc(PTR_SIZE * VECTOR_INIT_SIZE);
    v->items_count = 0;
}

void *vector_get(vector_t *v, uint64_t index) {
    if (index < v->items_count) {
        return v->items[index];
    }
    return (void *) 0;
}

void vector_resize(vector_t *v, uint64_t new_size) {
    v->max_items = new_size;
    v->items = krealloc(v->items, new_size * PTR_SIZE);
}

void vector_add(vector_t *v, void *new_item) {
    /* If vector is full, add 4 slots */
    if (v->max_items == v->items_count) {
        vector_resize(v, v->max_items + 4);
    }
    v->items[v->items_count++] = new_item;
}

void vector_delete(vector_t *v, uint64_t index) {
    if (index >= v->items_count) {
        return;
    }

    v->items[index] = 0;

    for (uint64_t i = index; i < v->items_count - 1; i++) {
        v->items[i] = v->items[i + 1];
        v->items[i + 1] = 0;
    }

    v->items_count--;

    if (v->items_count > 0 && v->items_count == v->max_items / 2) {
        vector_resize(v, v->max_items / 2);
    }
}

void **vector_items(vector_t *v) {
    return v->items;
}

void vector_uninit(vector_t *v) {
    kfree(v->items);
    v->items_count = 0;
    v->max_items = 0;
}