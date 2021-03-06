#ifndef KLIBC_LINKED_LIST_T
#define KLIBC_LINKED_LIST_T

#define UNCHAIN_LINKED_LIST(elem)                       \
    if (elem->prev) elem->prev->next = elem->next;      \
    if (elem->next) elem->next->prev = elem->prev;

#define CHAIN_LINKED_LIST(after, new_elem)              \
    new_elem->next = after->next;                       \
    new_elem->prev = after;                             \
    if(new_elem->next) new_elem->next->prev = new_elem; \
    after->next = new_elem;

#define CHAIN_LINKED_LIST_BEFORE(before, new_elem)      \
    if (before->prev) before->prev->next = new_elem;    \
    new_elem->prev = before->prev;                      \
    new_elem->next = before;                            \
    before->prev = new_elem;

#endif