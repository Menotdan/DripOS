#include <stdint.h>
#include "drivers/serial.h"

// Kernel main function, execution starts here :D
void kmain() {
    sprint("\nThis is a test :D");
    while (1) {
        asm volatile("hlt");
    }
}