#include "strhashmap.h"
#include "klibc/stdlib.h"
#include "klibc/linked_list.h"
#include "klibc/string.h"

#include "drivers/serial.h"

static uint64_t str_get_bucket_from_hash(uint64_t hash) {
    return hash % STRHASHMAP_BUCKET_SIZE;
}

static uint64_t get_hash_from_str(char *str) {
    uint64_t hash = 5381;
    int c;

    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}

strhashmap_t *init_strhashmap() {
    strhashmap_t *ret = kcalloc(sizeof(strhashmap_t));
    return ret;
}

strhashmap_elem_t *strhashmap_get_elem_dat(strhashmap_t *hashmap, char *key) {
    lock(hashmap->hashmap_lock);
    strhashmap_elem_t *ret = (strhashmap_elem_t *) 0;


    uint64_t bucket = str_get_bucket_from_hash(get_hash_from_str(key));
    strhashmap_elem_t *cur_elem = hashmap->buckets[bucket].elements;
    while (cur_elem) {
        if (strcmp(cur_elem->key, key) == 0 && atomic_inc(&cur_elem->ref_count)) {
            ret = cur_elem;
            goto done;
        }

        cur_elem = cur_elem->next;
    }

done:
    unlock(hashmap->hashmap_lock);
    return ret;
}

static void strhashmap_unref_elem(strhashmap_elem_t *elem) {
    if (!atomic_dec(&elem->ref_count)) {
        // If no more refs exist, remove the elem
        UNCHAIN_LINKED_LIST(elem);
        kfree(elem); // Free the element
    }
}

void strhashmap_remove_elem(strhashmap_t *hashmap, char *key) {
    strhashmap_elem_t *elem = strhashmap_get_elem_dat(hashmap, key);
    if (elem) {
        strhashmap_unref_elem(elem); // Unref for the time we referenced the element to get it
        strhashmap_unref_elem(elem); // Unref for the 1 we set refcount to when the element is created
    }
} 

void *strhashmap_get_elem(strhashmap_t *hashmap, char *key) {
    strhashmap_elem_t *elem = strhashmap_get_elem_dat(hashmap, key);
    if (elem) {
        void *data = elem->data;
        strhashmap_unref_elem(elem);
        return data;
    } else {
        return (void *) 0;
    }
}

void strhashmap_set_elem(strhashmap_t *hashmap, char *key, void *data) {
    strhashmap_elem_t *elem = strhashmap_get_elem_dat(hashmap, key);
    lock(hashmap->hashmap_lock);
    if (elem) {
        elem->data = data;
        strhashmap_unref_elem(elem);
    } else {
        uint64_t bucket = str_get_bucket_from_hash(get_hash_from_str(key));
        strhashmap_elem_t *elem = kcalloc(sizeof(strhashmap_elem_t));
        elem->data = data;
        elem->ref_count = 1;

        uint64_t len = strlen(key);
        elem->key = kcalloc(len + 1);
        strcpy(key, elem->key);

        if (hashmap->buckets[bucket].elements) {
            CHAIN_LINKED_LIST(hashmap->buckets[bucket].elements, elem);
        } else {
            hashmap->buckets[bucket].elements = elem;
        }
    }
    unlock(hashmap->hashmap_lock);
}