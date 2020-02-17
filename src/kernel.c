#include <stdint.h>
#include <stddef.h>
#include "mm/pmm.h"
#include "sys/apic.h"
#include "sys/int/isr.h"
#include "klibc/string.h"
#include "drivers/pit.h"
#include "drivers/serial.h"
#include "multiboot.h"

// Kernel main function, execution starts here :D
void kmain(multiboot_info_t *mboot_dat) {
    if (mboot_dat) {
        sprintf("[DRIPOS]: Setting up memory bitmaps");
        pmm_memory_setup(mboot_dat);
    }

    sprintf("\n[DRIPOS]: Configuring LAPICs and IOAPIC routing");
    configure_apic();
    sprintf("\n[DRIPOS]: Registering interrupts and setting interrupt flag");
    configure_idt();
    sprintf("\n[DRIPOS]: Setting timer speed to 1000 hz");
    set_pit_freq();
    while (1) {
        asm volatile("hlt");
    }
}