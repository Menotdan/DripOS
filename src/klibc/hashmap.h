#ifndef KLIBC_HASHMAP_H
#define KLIBC_HASHMAP_H
#include <stdint.h>
#include "klibc/lock.h"

#define HASHMAP_BUCKET_SIZE 200

typedef struct {
    hashmap_bucket_t buckets[HASHMAP_BUCKET_SIZE];
    lock_t hashmap_lock;
} hashmap_t;

hashmap_t *init_hashmap();
void *hashmap_get_elem(hashmap_t *hashmap, uint64_t hash);
void hashmap_set_elem(hashmap_t *hashmap, uint64_t key, void *data);
void hashmap_remove_elem(hashmap_t *hashmap, uint64_t key);

#endif