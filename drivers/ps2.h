#include "../cpu/idt.h"
#include "../cpu/isr.h"
#include "../cpu/ports.h"
#include "../libc/function.h"
#include <serial.h>
#include <string.h>

uint8_t err;
void init_ps2();
char get_scancode();