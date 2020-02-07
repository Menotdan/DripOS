#include <stdint.h>
#include "klibc/string.h"
#include "drivers/serial.h"
#include "mm/pmm.h"
#include "multiboot.h"

// Kernel main function, execution starts here :D
void kmain(multiboot_info_t *mboot_dat) {
    if (mboot_dat) {
        pmm_memory_setup(mboot_dat);
    }

    sprintf("\nTest alloc: %lx", pmm_alloc(0x2000));
    sprintf("\nTest alloc 2: %lx", pmm_alloc(1));

    while (1) {
        asm volatile("hlt");
    }
}