#ifndef KLIBC_DYNARRAY_H
#define KLIBC_DYNARRAY_H
#include "klibc/lock.h"

#define new_local_dynarray(name, type)                   \
    lock_t dynarray_##name##_lock = 0;                   \
    typedef struct dynarray_##name## {                   \
        type data;                                       \
        uint8_t present;                                 \
    } dynarray_##name##_t;                               \
    uint64_t dynarray_##name##_item_max = 4;             \
    dynarray_##name##_t *##name## = kcalloc(sizeof(dynarray_##name##_t) * 4);

#define new_dynarray(name, type)                         \
    lock_t dynarray_##name##_lock = 0;                   \
    typedef struct dynarray_##name## {                   \
        type data;                                       \
        uint8_t present;                                 \
    } dynarray_##name##_t;                               \
    uint64_t dynarray_##name##_item_max = 0;             \
    dynarray_##name##_t *##name##;

#define init_global_dynarray(name)                       \
    spinlock_lock(&dynarray_##name##_lock);              \
    ##name## = kcalloc(sizeof(dynarray_##name##_t) * 4); \
    spinlock_unlock(&dynarray_##name##_lock);

#define dynarray_add(name, data)                         \
    spinlock_lock(&dynarray_##name##_lock);              \
    uint8_t found_data_##name##_dynarray = 0;            \
    for (uint64_t i = 0; i < dynarray_##name##_item_max; i++) { \
        if (!##name##[i].present) {                      \
            ##name##[i].data = data;                     \
            ##name##[i].present = 1;                     \
        } else {                                         \
            found_data_##name##_dynarray = 1;            \
        }                                                \
    }                                                    \
    if (!found_data_##name##_dynarray) {                 \
        ##name## = krealloc(##name##, sizeof(dynarray_##name##_t) * (dynarray_##name##_item_max + 4)); \
        uint64_t ##name##_dynarray_offset = sizeof(dynarray_##name##_t) * (dynarray_##name##_item_max); \
        uint8_t *##name##_dynarray_clear_buffer = (uint8_t *) ##name## + ##name##_dynarray_offset; \
        memset(##name##_dynarray_clear_buffer, 0, 4 * sizeof(dynarray_##name##_t)); \
        ##name##[dynarray_##name##_item_max].data = data; \
        dynarray_##name##_item_max += 4                  \
    }                                                    \
    spinlock_unlock(&dynarray_##name##_lock);

#define dynarray_remove(name, index)

#define dynarray_get_elem(name, index)

#endif
