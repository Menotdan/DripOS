#include <stdint.h>
#include <stddef.h>
#include "klibc/string.h"
#include "drivers/serial.h"
#include "sys/apic.h"
#include "mm/pmm.h"
#include "multiboot.h"
#include "sys/int/isr.h"
#include "drivers/pit.h"

// Kernel main function, execution starts here :D
void kmain(multiboot_info_t *mboot_dat) {
    if (mboot_dat) {
        sprintf("[DRIPOS]: Setting up memory bitmaps");
        pmm_memory_setup(mboot_dat);
    }

    sprintf("\n[DRIPOS]: Setting timer speed to 1000 hz");
    set_pit_freq();
    sprintf("\n[DRIPOS]: Configuring LAPICs and IOAPIC routing");
    configure_apic();
    sprintf("\n[DRIPOS]: Registering interrupts and setting interrupt flag");
    configure_idt();
    //uint64_t oof = 0;
    while (1) {
        //sprintf("\noof: %lx", oof);
        //oof++;
        asm volatile("hlt");
    }
}