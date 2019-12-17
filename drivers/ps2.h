#include "../cpu/isr.h"
#include "../cpu/idt.h"
#include "../cpu/ports.h"
#include <serial.h>
#include "../libc/string.h"
#include "../libc/function.h"

uint8_t err;
void init_ps2();
char get_scancode();