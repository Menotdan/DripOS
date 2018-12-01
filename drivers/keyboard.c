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
static int prevcode = 0;
static int times;
static bool send = true;

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
		if ((int)scancode != prevcode) {
			times = 0;
			prevcode = (int)scancode;
			send = true;
		} else {
			if(scancode != BACKSPACE) {
				send = false;
				times += 1;
			} else {
				send = true;
				times = 0;
			}
		}
    }
	if (times >= 2) {
		send = true;
		times = 0;
	}
	if (scancode == ENTER || scancode == BACKSPACE || scancode == LSHIFT || scancode == RSHIFT) {
		send = true;
		times = 0;
	}
	if (send == true) {
		key_handler();
		//kprint("sent");
	}
    UNUSED(regs);
}

void init_keyboard() {
   register_interrupt_handler(IRQ1, keyboard_callback); 
}