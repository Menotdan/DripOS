#include <stdlib.h>
#include <stdint.h>
#include "../libc/mem.h"
#include "../drivers/serial.h"
#include <debug.h>

void exit() {
    /* Exectute an exit syscall */
    asm volatile("\
                mov $0, %eax\n\
                int $0x80");
}

void malloc(uint64_t size) {
    kmalloc(size);
}