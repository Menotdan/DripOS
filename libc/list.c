#include <serial.h>
#include <libc.h>
#include <stdio.h>
#include "list.h"

//void add_at_index(uint32_t index, uint32_t data, list_t *l) {

//}

// List for uint32_t
list_t *new_list(uint32_t start_val) {
    list_t *ret = kmalloc(sizeof(list_t));
    //sprint("\nAddr: ");
    //sprint_uint(ret);
    ret->elements = 0;
    ret->elements += 1;
    ret->head = 0;
    ret->tail = 0;
    ret->tail = kmalloc(sizeof(node_t));
    ret->tail->data = start_val;
    ret->head = ret->tail;
    return ret;
}

void add_at_end(uint32_t data, list_t *l) {
    node_t *cur_head; // Current head
    node_t *new_data = kmalloc(sizeof(node_t)); // Get new memory for the new element
    new_data->next = 0;
    new_data->data = data; // Set the data
    cur_head = l->head; // Set the current head
    l->head = new_data; // Change the head of the list
    cur_head->next = new_data; // Point the previous head at the current head
    l->elements += 1; // Increment the elements counter
}

void remove_at_end(list_t *l) {
    node_t *new_head = l->tail;
    node_t *cur_head = l->head;
    for (uint32_t e = 0; e < (l->elements-2); e++) {
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
        free(new_head->next);
        new_head->next = 0;
    }
    l->head = new_head;
    cur_head->next = l->head;
    l->elements -= 1;
}

void destroy_list(list_t *l) {
    node_t *current = l->tail;
    node_t *next;
    while (current) {
        next = current->next;
        free(current);
        current = next;
    }
    free(l);
}

uint32_t data_at_index(uint32_t index, list_t *l) {
    struct node *cur = l->tail;
    if (index < l->elements) {
        for (uint32_t i = 0; i<index-1; i++) {
            if (!(cur->next)) {
                sprint("\nTest out\n");
                break;
            }
            cur = cur->next;
            if (cur == l->head) {
                sprint("\nHit the end of the list\n");
            }
        }
    }
    return cur->data;
}

uint32_t data_at_end(list_t *l) {
    return l->head->data;
}

// List for directory entries

// dir_list_t *new_dir_list(dir_entry_t start_val) {
//     dir_list_t *ret = kmalloc(sizeof(dir_list_t));
//     //sprint("\nAddr: ");
//     //sprint_uint(ret);
//     ret->elements = 0;
//     ret->elements += 1;
//     ret->head = 0;
//     ret->tail = 0;
//     ret->tail = kmalloc(sizeof(dir_node_t));
//     ret->tail->data = start_val;
//     ret->head = ret->tail;
//     return ret;
// }

// void add_at_end_dir(dir_entry_t data, dir_list_t *l) {
//     dir_node_t *cur_head; // Current head
//     dir_node_t *new_data = kmalloc(sizeof(dir_node_t)); // Get new memory for the new element
//     new_data->next = 0;
//     new_data->data = data; // Set the data
//     cur_head = l->head; // Set the current head
//     l->head = new_data; // Change the head of the list
//     cur_head->next = new_data; // Point the previous head at the current head
//     l->elements += 1; // Increment the elements counter
// }

// void remove_at_end_dir(dir_list_t *l) {
//     dir_node_t *new_head = l->tail;
//     dir_node_t *cur_head = l->head;
//     for (uint32_t e = 0; e < (l->elements-2); e++) {
//         if (new_head->next) {
//             new_head = new_head->next;
//         } else {
//             sprintd("Test out two");
//         }
//     }
//     if (new_head == l->tail) {
//         destroy_list_dir(l);
//         return;
//     }
//     if (new_head->next) {
//         free(new_head->next, sizeof(dir_node_t));
//         new_head->next = 0;
//     }
//     l->head = new_head;
//     cur_head->next = l->head;
//     l->elements -= 1;
// }

// void destroy_list_dir(dir_list_t *l) {
//     dir_node_t *current = l->tail;
//     dir_node_t *next;
//     while (current) {
//         next = current->next;
//         free(current, sizeof(node_t));
//         current = next;
//     }
//     free(l, sizeof(dir_list_t));
// }

// dir_entry_t data_at_index_dir(uint32_t index, dir_list_t *l) {
//     struct dir_node *cur = l->tail;
//     if (index < l->elements) {
//         for (uint32_t i = 0; i<index-1; i++) {
//             if (!(cur->next)) {
//                 sprint("\nTest out\n");
//                 break;
//             }
//             cur = cur->next;
//             if (cur == l->head) {
//                 sprint("\nHit the end of the list\n");
//             }
//         }
//     }
//     return cur->data;
// }

// dir_entry_t data_at_end_dir(dir_list_t *l) {
//     return l->head->data;
// }