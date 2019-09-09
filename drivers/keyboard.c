#include "keyboard.h"
#include "../cpu/ports.h"
#include "../cpu/isr.h"
#include "screen.h"
#include "../libc/string.h"
#include "../libc/function.h"
#include "../kernel/kernel.h"
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
static uint32_t pAddr;

static void keyboard_callback(registers_t *regs) {
    /* The PIC leaves us the scancode in port 0x60 */
    uint8_t scancode = port_byte_in(0x60);
	
	//kprint_int(scancode);

	bool iskeyup = false;
	if (scancode >= KEYUPOFFSET) {
		iskeyup = true;
		scancode -= KEYUPOFFSET;
	}
	//kprint("\nKey Buffer before: ");
	//kprint(key_buffer);
	key_handler(scancode, iskeyup);
	//kprint("\nKey Buffer after: ");
	//kprint(key_buffer);
    UNUSED(regs);
}

void init_keyboard() {
	register_interrupt_handler(IRQ1, keyboard_callback);
}