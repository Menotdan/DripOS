#include "hashmap.h"
#include "klibc/stdlib.h"

void *hashmap_get_elem(hashmap_t *hashmap, uint64_t hash) {
    lock(&hashmap->hashmap_lock);
    uint64_t bucket = hash / HASHMAP_BUCKET_SIZE;
    uint64_t bucket_item = hash % HASHMAP_BUCKET_SIZE;

    if (bucket > hashmap->bucket_highest) {
        unlock(&hashmap->hashmap_lock);
        return (void *) 0;
    }

    if (hashmap->buckets[bucket].elements == (void *) 0) {
        unlock(&hashmap->hashmap_lock);
        return (void *) 0;
    }

    void *item = hashmap->buckets[bucket].elements[bucket_item].data;
    unlock(&hashmap->hashmap_lock);
    return item;
}

void hashmap_set_elem(hashmap_t *hashmap, uint64_t key, void *data) {
    lock(&hashmap->hashmap_lock);
    uint64_t bucket = key / HASHMAP_BUCKET_SIZE;
    uint64_t bucket_item = key % HASHMAP_BUCKET_SIZE;

    if (bucket >= hashmap->bucket_highest) {
        hashmap->buckets = krealloc(hashmap->buckets, sizeof(hashmap_bucket_t) * bucket + 1);
        hashmap->bucket_highest = bucket + 1;
        hashmap->buckets[bucket].elements = kcalloc(sizeof(hashmap_elem_t) * HASHMAP_BUCKET_SIZE);
    }

    if (!hashmap->buckets[bucket].elements) {
        hashmap->buckets[bucket].elements = kcalloc(sizeof(hashmap_elem_t) * HASHMAP_BUCKET_SIZE);
    }

    hashmap->buckets[bucket].elements[bucket_item].data = data;
    hashmap->buckets[bucket].elements[bucket_item].key = key;

    unlock(&hashmap->hashmap_lock);
}

void hashmap_remove_elem(hashmap_t *hashmap, uint64_t key) {
    lock(&hashmap->hashmap_lock);
    uint64_t bucket = key / HASHMAP_BUCKET_SIZE;
    uint64_t bucket_item = key % HASHMAP_BUCKET_SIZE;

    if (bucket > hashmap->bucket_highest) {
        unlock(&hashmap->hashmap_lock);
        return;
    }

    if (hashmap->buckets[bucket].elements == (void *) 0) {
        unlock(&hashmap->hashmap_lock);
        return;
    }

    hashmap->buckets[bucket].elements[bucket_item].data = 0;
    hashmap->buckets[bucket].elements[bucket_item].key = 0;

    uint8_t found_present = 0;
    for (uint64_t i = 0; i < HASHMAP_BUCKET_SIZE; i++) {
        if (hashmap->buckets[bucket].elements[i].data || hashmap->buckets[bucket].elements[i].key) {
            found_present = 1;
            break;
        }
    }

    if (!found_present) {
        kfree(hashmap->buckets[bucket].elements);
        hashmap->buckets[bucket].elements = 0;
    }

    unlock(&hashmap->hashmap_lock);
}