#include <stdint.h>
#include "klibc/string.h"
#include "drivers/serial.h"
#include "multiboot.h"

// Kernel main function, execution starts here :D
void kmain(multiboot_info_t *mboot_dat) {
    if (mboot_dat) {
        sprint("\nMultiboot header exists!");
    }
    sprint("\nThis is a test :D");
    sprintf("\nTest printf: %lx", 0x12345);
    sprintf("\nTest printf 2: %lu", (uint64_t) 101010);
    sprintf("\nTest printf 3: %ld", (int64_t) -101010);
    while (1) {
        asm volatile("hlt");
    }
}