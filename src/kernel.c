#include <stdint.h>
#include <stddef.h>
#include "mm/pmm.h"
#include "io/ports.h"
#include "sys/apic.h"
#include "sys/int/isr.h"
#include "drivers/pit.h"
#include "drivers/serial.h"
#include "drivers/vesa.h"
#include "drivers/tty/tty.h"
#include "multiboot.h"

// Kernel main function, execution starts here :D
void kmain(multiboot_info_t *mboot_dat) {
    tty_init(&base_tty, 8, 8);
    if (mboot_dat) {
        kprintf("[DripOS]: Setting up memory bitmaps");
        pmm_memory_setup(mboot_dat);
    }

    kprintf("\n[DripOS] Configuring LAPICs and IOAPIC routing");
    configure_apic();
    sprintf("\n[DripOS] Registering interrupts and setting interrupt flag");
    configure_idt();
    sprintf("\n[DripOS] Setting timer speed to 1000 hz");
    set_pit_freq();
    sprintf("\n[DripOS] Setting up TTY");

    while (1) {
        asm volatile("hlt");
    }
}