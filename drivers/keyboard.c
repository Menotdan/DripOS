#include "keyboard.h"
#include "../cpu/ports.h"
#include "../cpu/isr.h"
#include "screen.h"
#include "../libc/string.h"
#include "../libc/function.h"
#include "../kernel/kernel.h"
#include "stdin.h"
#include <stdbool.h>


#define BACKSPACE 0x0E
#define ENTER 0x1C
#define LSHIFT 0x2A
#define RSHIFT 0x36



#define SC_MAX 57

bool keydown[256];

static void keyboard_callback(registers_t regs) {
    /* The PIC leaves us the scancode in port 0x60 */
    u8 scancode = port_byte_in(0x60);
	

	bool iskeyup = false;
	if (scancode >= 0x80) {
		iskeyup = true;
		scancode -= 0x80;
	}

    if (scancode > SC_MAX) return;
	if (iskeyup == true) {
		keydown[(int)scancode] = false;
	} else {
		keydown[(int)scancode] = true;
    }
	key_handler();
    UNUSED(regs);
}

void init_keyboard() {
   register_interrupt_handler(IRQ1, keyboard_callback); 
}