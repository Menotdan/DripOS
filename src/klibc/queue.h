#ifndef KLIBC_QUEUE_H
#define KLIBC_QUEUE_H
#include "klibc/stdlib.h"
#include "klibc/string.h"
#include "linked_list.h"
#include "lock.h"
#include <stdint.h>

#define new_queue(name,elem_type)       \
    typedef struct name##_queue {       \
        elem_type *elem;                \
        struct name##_queue *next;      \
        struct name##_queue *prev;      \
    } name##_queue_t;                   \
    lock_t name##_queue_lock = {0,0,0}; \
    name##_queue_t *name = 0;

#define queue_insert_at_front(name, elem)              \
    lock(name##_queue_lock);                           \
    if (!name) {                                       \
        name = kcalloc(sizeof(name##_queue_t));        \
        name->elem = kcalloc(sizeof(elem));            \
        memcpy(&elem, name->elem, sizeof(elem));       \
    } else {                                           \
        name##_queue_t *new_queue_##name = kcalloc(sizeof(name##_queue_t)); \
        CHAIN_LINKED_LIST_BEFORE(name, new_queue_##name); \
        name = name->prev;                             \
        name->elem = kcalloc(sizeof(elem));            \
        memcpy(&elem, name->elem, sizeof(elem));       \
    }                                                  \
    unlock(name##_queue_lock);

#define queue_insert_at_back(name, elem)               \
    lock(name##_queue_lock);                           \
    name##_queue_t *cur = name;                        \
    if (!name) {                                       \
        name = kcalloc(sizeof(name##_queue_t));        \
        name->elem = kcalloc(sizeof(elem));            \
        memcpy(&elem, name->elem, sizeof(elem));       \
    } else {                                           \
        while(cur->next) {                             \
            cur = cur->next;                           \
        }                                              \
        name##_queue_t *new_elem = kcalloc(sizeof(name##_queue_t)); \
        CHAIN_LINKED_LIST(cur, new_elem);              \
        new_elem->elem = kcalloc(sizeof(elem));        \
        memcpy(&elem, name->elem, sizeof(elem));       \
    }                                                  \
    unlock(name##_queue_lock);

#define pop_queue(name, type) {(     \
    lock(name##_queue_lock);     \
    type *return_dat = 0;        \
    if (name) {                  \
        return_dat = name->data; \
        kfree(name);             \
        name = name->next;       \
    }                            \
    unlock(name##_queue_lock);   \
    return_dat; )}

#endif