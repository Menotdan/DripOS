#ifndef KLIBC_DYNARRAY_H
#define KLIBC_DYNARRAY_H
#include "klibc/lock.h"
#include "klibc/stdlib.h"
#include <stddef.h>

#define dynarray_new(type, name) \
    static struct { \
        int refcount; \
        int present; \
        type data; \
    } **name; \
    static size_t name##_i = 0; \
    static lock_t name##_lock = 0;

#define public_dynarray_new(type, name) \
    struct __##name##_struct **name; \
    size_t name##_i = 0; \
    lock_t name##_lock = 0;

#define public_dynarray_prototype(type, name) \
    struct __##name##_struct { \
        int refcount; \
        int present; \
        type data; \
    }; \
    extern struct __##name##_struct **name; \
    extern size_t name##_i; \
    extern lock_t name##_lock;

#define dynarray_remove(dynarray, element) ({ \
    __label__ out; \
    int ret; \
    spinlock_lock(&dynarray##_lock); \
    if (!dynarray[element]) { \
        ret = -1; \
        goto out; \
    } \
    ret = 0; \
    dynarray[element]->present = 0; \
    if (!atomic_dec(&dynarray[element]->refcount)) { \
        kfree(dynarray[element]); \
        dynarray[element] = 0; \
    } \
out: \
    spinlock_unlock(&dynarray##_lock); \
    ret; \
})

#define dynarray_unref(dynarray, element) ({ \
    spinlock_lock(&dynarray##_lock); \
    if (dynarray[element] && !atomic_dec(&dynarray[element]->refcount)) { \
        kfree(dynarray[element]); \
        dynarray[element] = 0; \
    } \
    spinlock_unlock(&dynarray##_lock); \
})

#define dynarray_getelem(type, dynarray, element) ({ \
    spinlock_lock(&dynarray##_lock); \
    type *ptr = NULL; \
    if (dynarray[element] && dynarray[element]->present) { \
        ptr = &dynarray[element]->data; \
        atomic_inc(&dynarray[element]->refcount); \
    } \
    spinlock_unlock(&dynarray##_lock); \
    ptr; \
})

#define dynarray_add(type, dynarray, element) ({ \
    __label__ fnd; \
    __label__ out; \
    int ret = -1; \
        \
    spinlock_lock(&dynarray##_lock); \
        \
    size_t i; \
    for (i = 0; i < dynarray##_i; i++) { \
        if (!dynarray[i]) \
            goto fnd; \
    } \
        \
    dynarray##_i += 256; \
    void *tmp = krealloc(dynarray, dynarray##_i * sizeof(void *)); \
    if (!tmp) \
        goto out; \
    dynarray = tmp; \
        \
fnd: \
    dynarray[i] = kalloc(sizeof(**dynarray)); \
    if (!dynarray[i]) \
        goto out; \
    dynarray[i]->refcount = 1; \
    dynarray[i]->present = 1; \
    dynarray[i]->data = *element; \
        \
    ret = i; \
        \
out: \
    spinlock_unlock(&dynarray##_lock); \
    ret; \
})

#define dynarray_search(type, dynarray, i_ptr, cond, index) ({ \
    __label__ fnd; \
    __label__ out; \
    type *ret = NULL; \
        \
    spinlock_lock(&dynarray##_lock); \
        \
    size_t i; \
    size_t j = 0; \
    for (i = 0; i < dynarray##_i; i++) { \
        if (!dynarray[i] || !dynarray[i]->present) \
            continue; \
        type *elem = &dynarray[i]->data; \
        if ((cond) && j++ == (index)) \
            goto fnd; \
    } \
    goto out; \
        \
fnd: \
    ret = &dynarray[i]->data; \
    atomic_inc(&dynarray[i]->refcount); \
    *(i_ptr) = i; \
        \
out: \
    spinlock_unlock(&dynarray##_lock); \
    ret; \
})

#endif