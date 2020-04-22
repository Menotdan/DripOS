#ifndef KLIBC_RANGEMAP_H
#define KLIBC_RANGEMAP_H
#include <stdint.h>

typedef struct rangemap {
    /* Start and end of the numbers the map represents */
    uint64_t start;
    uint64_t end;
    struct rangemap *next;
    struct rangemap *prev;
} rangemap_t;

void rangemap_add_range(rangemap_t *base_entry, uint64_t range_start, uint64_t range_end);
uint64_t rangemap_find_free_area(rangemap_t *rangemap, uint64_t size);
void rangemap_mark_free(rangemap_t *rangemap, uint64_t addr);
uint64_t rangemap_get_entry_size(rangemap_t *rangemap, uint64_t addr);
uint8_t rangemap_entry_present(rangemap_t *rangemap, uint64_t addr);

#endif