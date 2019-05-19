#include "keyboard.h"
#include "../cpuPt2/ports.h"
#include "../cpuPt2/isr.h"
#include "screen.h"
#include "../libcPt2/string.h"
#include "../libcPt2/function.h"
#include "../kernelPt2/kernel.h"
#include "stdin.h"
#include <stdbool.h>
#include <stdint.h>

#define BACKSPACE 0x0E
#define ENTER 0x1C
#define LSHIFT 0x2A
#define RSHIFT 0x36
#define LARROW 0x6B
#define RARROW 0x6D
#define KEYUPOFFSET 0x80

#define SC_MAX 57


static int prevcode = 0;
static int times;
static bool send = true;

static void keyboard_callback(registers_t regs) {
    /* The PIC leaves us the scancode in port 0x60 */
    uint8_t scancode = port_byte_in(0x60);
	
	//kprint_int(scancode);

	bool iskeyup = false;
	if (scancode >= KEYUPOFFSET) {
		iskeyup = true;
		scancode -= KEYUPOFFSET;
	}

	key_handler(scancode, iskeyup);
    UNUSED(regs);
}

void init_keyboard() {
   register_interrupt_handler(IRQ1, keyboard_callback); 
}