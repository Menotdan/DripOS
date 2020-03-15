#ifndef KLIBC_HASHMAP_H
#define KLIBC_HASHMAP_H
#include <stdint.h>
#include "klibc/lock.h"

#define HASHMAP_BUCKET_SIZE 1000

typedef struct {
    uint64_t key;
    void *data;
} hashmap_elem_t;

typedef struct {
    hashmap_elem_t *elements;
} hashmap_bucket_t;

typedef struct {
    hashmap_bucket_t *buckets;
    uint64_t bucket_highest;
    lock_t hashmap_lock;
} hashmap_t;

#endif