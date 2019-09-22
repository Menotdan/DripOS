#include <serial.h>
#include <libc.h>
#include <stdio.h>
#include "list.h"

//void add_at_index(uint32_t index, uint32_t data, list_t *l) {

//}
list_t *new_list(uint32_t start_val) {
    list_t *ret = kmalloc(sizeof(list_t));
    ret->tail = kmalloc(sizeof(node_t));
    ret->tail->data = start_val;
    ret->elements += 1;
}

void add_at_end(uint32_t data, list_t *l) {
    node_t *cur_head; // Current head
    node_t *new_data = kmalloc(sizeof(node_t)); // Get new memory for the new element
    new_data->data = data; // Set the data
    cur_head = l->head; // Set the current head
    l->head = new_data; // Change the head of the list
    cur_head->next = new_data; // Point the previous head at the current head
    l->elements += 1; // Increment the elements counter
}

void remove_at_end(list_t *l) {
    node_t *new_head = l->tail;
    for (uint32_t e = 0; e < (l->elements - 2); e++) {
        if (new_head->next) {
            new_head = new_head->next;
        } else {
            sprintd("Test out two");
        }
    }
    if (new_head == l->tail) {
        destroy_list(l);
        return;
    }
    if (new_head->next) {
        free(new_head->next, sizeof(node_t));
        new_head->next = 0;
    }
    l->head = new_head;
}

void destroy_list(list_t *l) {
    node_t *current = l->tail;
    node_t *next;
    while (current) {
        next = current->next;
        free(current, sizeof(node_t));
        current = next;
    }
    free(l, sizeof(list_t));
}

uint32_t data_at_index(uint32_t index, list_t *l) {
    struct node *cur = l->tail;
    if (index < l->elements) {
        for (uint32_t i = 0; i<index; i++) {
            if (!(cur->next)) {
                sprintd("Test out");
                break;
            }
            cur = cur->next;
        }
    }
    return cur->data;
}

uint32_t data_at_end(list_t *l) {
    return l->head->data;
}