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
    if (mboot_dat) {
        uint16_t divisor = 2386;

        port_outb(0x43, 0xb6);
        port_outb(0x42, (uint8_t)(divisor));
        port_outb(0x42, (uint8_t)(divisor >> 8));

        uint8_t tmp1 = port_inb(0x61);
        if (tmp1 != (tmp1 | 3)) {
            port_outb(0x61, tmp1 | 3);
        }
        for (uint32_t i = 0; i < 0xFFFFFFF; i++) {asm volatile("pause");}
        uint8_t tmp2 = port_inb(0x61) & 0xFC;
        port_outb(0x61, tmp2);
        for (uint32_t i = 0; i < 0xFFFFFFF; i++) {asm volatile("pause");}
        sprintf("[DripOS]: Setting up memory bitmaps");
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