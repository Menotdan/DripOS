#include <stdint.h>
#include <stddef.h>
#include "klibc/string.h"
#include "drivers/serial.h"
#include "sys/apic.h"
#include "mm/pmm.h"
#include "multiboot.h"

// Kernel main function, execution starts here :D
void kmain(multiboot_info_t *mboot_dat) {
    if (mboot_dat) {
        pmm_memory_setup(mboot_dat);
    }

    sprintf("\nTest alloc: %lx", pmm_alloc(0x2000));
    sprintf("\nTest alloc 2: %lx", pmm_alloc(1));

    sprintf("\nConfiguring APIC");
    configure_apic();
    

    while (1) {
        asm volatile("hlt");
    }
}