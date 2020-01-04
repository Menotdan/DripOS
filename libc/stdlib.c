#include <stdlib.h>
#include <stdint.h>
#include "../libc/mem.h"

void exit() {
    /* Exectute an exit syscall */
    asm volatile("\
                mov $0, %eax\n\
                int $0x80");
}

void malloc(uint32_t size) {
    kmalloc(size);
}