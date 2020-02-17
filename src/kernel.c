#include <stdint.h>
#include <stddef.h>
#include "mm/pmm.h"
#include "sys/apic.h"
#include "sys/int/isr.h"
#include "drivers/pit.h"
#include "drivers/serial.h"
#include "drivers/vesa.h"
#include "multiboot.h"

// Kernel main function, execution starts here :D
void kmain(multiboot_info_t *mboot_dat) {
    if (mboot_dat) {
        sprintf("[DRIPOS]: Setting up memory bitmaps");
        pmm_memory_setup(mboot_dat);
    }
   
    sprintf("\n[DripOS] Setting up display");
    init_vesa(mboot_dat);
    sprintf("\n[DripOS] Configuring LAPICs and IOAPIC routing");
    configure_apic();
    sprintf("\n[DripOS] Registering interrupts and setting interrupt flag");
    configure_idt();
    sprintf("\n[DripOS] Setting timer speed to 1000 hz");
    set_pit_freq();
    // uint8_t r = 255;
    // uint8_t g = 254;
    // uint8_t b = 255;
    // while (1) {
    //     color_t test = {r, g, b};
    //     for (uint64_t y = 0; y < vesa_display_info.height; y++) {
    //         for (uint64_t x = 0; x < vesa_display_info.width; x++) {
    //             put_pixel(x, y, test);
    //             test.r -= 1;
    //             test.g -= 2;
    //             test.b -= 4;
    //         }
    //     }
    //     r -= 1;
    //     g -= 3;
    //     b -= 1;
    // }

    while (1) {
        asm volatile("hlt");
    }
}