#ifndef MEM_H
#define MEM_H

#include "../cpu/types.h"
#include <stdint.h>

void memcpy(uint8_t *source, uint8_t *dest, int nbytes);
void memset(uint8_t *dest, uint8_t val, u32 len);

/* At this stage there is no 'free' implemented. */
u32 kmalloc(u32 size, int align, u32 *phys_addr);

#endif
