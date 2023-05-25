#ifndef KLIBC_HASHMAP_H
#define KLIBC_HASHMAP_H
#include <stdint.h>
#include "klibc/lock.h"

#define HASHMAP_BUCKET_SIZE 200
#define HASHMAP_ITERABLE(hashmap) for (uint64_t hashmap_bucket_i = 0; hashmap_bucket_i < HASHMAP_BUCKET_SIZE; hashmap_bucket_i++) { \
  hashmap_elem_t *hashmap_element_cur = hashmap->buckets[hashmap_bucket_i].elements; \
  if (!hashmap_element_cur) { \
   continue; \
  } \
  do { \

#define HASHMAP_ITERABLE_END \
    hashmap_element_cur = hashmap_element_cur->next; \
  } while (hashmap_element_cur); \
} \

#define HASHMAP_ITERABLE_GET (hashmap_element_cur)

#define HASHMAP_ITERABLE_DROP_ENTRY(hashmap) hashmap_remove_elem(hashmap, hashmap_element_cur->key);

typedef struct hashmap_elem {
    uint64_t key;
    void *data;
    struct hashmap_elem *next;
    struct hashmap_elem *prev;
    uint32_t ref_count;
} hashmap_elem_t;

typedef struct {
    hashmap_elem_t *elements;
} hashmap_bucket_t;

typedef struct {
    hashmap_bucket_t buckets[HASHMAP_BUCKET_SIZE];
    lock_t hashmap_lock;
} hashmap_t;

hashmap_t *init_hashmap();
void *hashmap_get_elem(hashmap_t *hashmap, uint64_t hash);
void hashmap_set_elem(hashmap_t *hashmap, uint64_t key, void *data);
void hashmap_remove_elem(hashmap_t *hashmap, uint64_t key);
void delete_hashmap(hashmap_t *hashmap);

#endif