#ifndef KLIBC_STRHASHMAP_H
#define KLIBC_STRHASHMAP_H
#include <stdint.h>
#include "klibc/lock.h"

#define STRHASHMAP_BUCKET_SIZE 200

typedef struct {
    strhashmap_bucket_t buckets[STRHASHMAP_BUCKET_SIZE];
    lock_t hashmap_lock;
} strhashmap_t;

strhashmap_t *init_strhashmap();
void *strhashmap_get_elem(strhashmap_t *hashmap, char *key);
void strhashmap_set_elem(strhashmap_t *hashmap, char *key, void *data);
void strhashmap_remove_elem(strhashmap_t *hashmap, char *key);

#endif