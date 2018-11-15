#ifndef MEM_H
#define MEM_H

#include "../cpu/types.h"

void terminal_init();
void terminal_input(char*);

/* At this stage there is no 'free' implemented. */
u32 kmalloc(u32 size, int align, u32 *phys_addr);

#endif
