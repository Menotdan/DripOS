#include "rangemap.h"
#include "klibc/stdlib.h"
#include "drivers/serial.h"

void rangemap_insert_new(rangemap_t *after, rangemap_t *new) {
    new->next = after->next;
    new->prev = after;
    if(new->next)
        new->next->prev = new;
    after->next = new;
}

void rangemap_remove(rangemap_t *remove) {
    if (remove->next) remove->next->prev = remove->prev; // Point to the previous
    if (remove->prev) remove->prev->next = remove->next; // Point to the next
    kfree(remove);
}

void rangemap_add_range(rangemap_t *base_entry, uint64_t range_start, uint64_t range_end) {
    rangemap_t *start_insert = base_entry;
    rangemap_t *current = base_entry;
    while (current) {
        if (current->end <= range_start) {
            start_insert = current;
        } else {
            break;
        }

        current = current->next;
    }

    rangemap_t *new = kcalloc(sizeof(rangemap_t));
    new->start = range_start;
    new->end = range_end;
    rangemap_insert_new(start_insert, new);
}

uint64_t rangemap_find_free_area(rangemap_t *rangemap, uint64_t size) {
    rangemap_t *current = rangemap;
    while (current) {
        if (!current->next) {
            if (0xFFFFFFFFFFFFFFFF - current->end >= size) {
                rangemap_add_range(rangemap, current->end, current->end + size);

                return current->end;
            }
        } else {
            if (current->next->start - current->end >= size) {
                rangemap_add_range(rangemap, current->end, current->end + size);

                return current->end;
            }
        }
        current = current->next;
    }

    // oops didn't find anything
    return 0xFFFFFFFFFFFFFFFF;
}

void rangemap_mark_free(rangemap_t *rangemap, uint64_t addr) {
    rangemap_t *current = rangemap;
    while (current) {
        if (current->start == addr && !(current == rangemap)) {
            rangemap_remove(current);

            return;
        }
        current = current->next;
    }
}

void print_ranges(rangemap_t *base) {
    rangemap_t *current = base;
    sprintf("\nEntries:");
    while (current) {
        sprintf("\n  entry %lx - %lx", current->start, current->end);
        current = current->next;
    }
}