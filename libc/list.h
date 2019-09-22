#ifndef LIST_H
#define LIST_H

#include <stdint.h>

typedef struct node
{
    uint32_t data;
    struct node *next;
} __attribute__((packed)) node_t;

typedef struct list
{
    node_t *tail;
    node_t *head;
    uint32_t elements;
} __attribute__((packed)) list_t;

void add_at_end(uint32_t data, list_t *l);
void remove_at_end(list_t *l);
void destroy_list(list_t *l);
list_t *new_list(uint32_t start_val);
uint32_t data_at_index(uint32_t index, list_t *l);
uint32_t data_at_end(list_t *l);

#endif