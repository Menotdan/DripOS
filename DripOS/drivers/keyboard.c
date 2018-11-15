#include "keyboard.h"
#include "../cpu/ports.h"
#include "../cpu/isr.h"
#include "screen.h"
#include "../libc/string.h"
#include "../libc/function.h"
#include "../kernel/kernel.h"
#include "../kernel/terminal.h"

#define BACKSPACE 0x0E
#define ENTER 0x1C
#define LSHIFT 0x2A
#define RSHIFT 0x36

int shift = 0;
static char key_buffer[2560];

#define SC_MAX 57
const char *sc_name[] = { "ERROR", "Esc", "1", "2", "3", "4", "5", "6",
    "7", "8", "9", "0", "-", "=", "Backspace", "Tab", "Q", "W", "E",
        "R", "T", "Y", "U", "I", "O", "P", "[", "]", "Enter", "Lctrl",
        "A", "S", "D", "F", "G", "H", "J", "K", "L", ";", "'", "`",
        "LShift", "\\", "Z", "X", "C", "V", "B", "N", "M", ",", ".",
        "/", "RShift", "Keypad *", "LAlt", "Spacebar"};
const char sc_ascii[] = { ' ', ' ', '1', '2', '3', '4', '5', '6',
    '7', '8', '9', '0', '-', '=', ' ', ' ', 'q', 'w', 'e', 'r', 't', 'y',
        'u', 'i', 'o', 'p', '[', ']', ' ', ' ', 'a', 's', 'd', 'f', 'g',
        'h', 'j', 'k', 'l', ';', '\'', '`', ' ', '\\', 'z', 'x', 'c', 'v',
        'b', 'n', 'm', ',', '.', '/', ' ', ' ', ' ', ' '};

const char sc_ascii_uppercase[] = { ' ', ' ', '!', '@', '#', '$', '%', '^',
    '&', '*', '(', ')', '_', '+', ' ', ' ', 'Q', 'W', 'E', 'R', 'T', 'Y',
        'U', 'I', 'O', 'P', '{', '}', ' ', ' ', 'A', 'S', 'D', 'F', 'G',
        'H', 'J', 'K', 'L', ':', '|', '~', ' ', '\\', 'Z', 'X', 'C', 'V',
        'B', 'N', 'M', '<', '>', '?', ' ', ' ', ' ', ' '};

static void keyboard_callback(registers_t regs) {
    /* The PIC leaves us the scancode in port 0x60 */
    u8 scancode = port_byte_in(0x60);

    if (scancode > SC_MAX) return;
    if (scancode == BACKSPACE) {
		if (uinlen > 0) {
			backspace(key_buffer);
			kprint_backspace();
			uinlen -= 1;
		}
    } else if (scancode == ENTER) {
        kprint("\n");
        terminal_input(key_buffer); /* kernel-controlled function */
		uinlen = 0;
        key_buffer[0] = '\0';
    } else if (scancode == LSHIFT || scancode == RSHIFT) {
		if (shift == 0) {
			shift = 1;
		} else {
			shift = 0;
		}
	} else {
		uinlen += 1;
		if (shift == 0) {
			char letter = sc_ascii[(int)scancode];
			/* Remember that kprint only accepts char[] */
			char str[2] = {letter, '\0'};
			append(key_buffer, letter);
			if (prompttype == 0) {
				kprint(str);
			} else {
				kprint("*");
			}
		} else {
			char letter = sc_ascii_uppercase[(int)scancode];
			/* Remember that kprint only accepts char[] */
			char str[2] = {letter, '\0'};
			append(key_buffer, letter);
			if (prompttype == 0) {
				kprint(str);
			} else {
				kprint("*");
			}
		}
    }
    UNUSED(regs);
}

void init_keyboard() {
   register_interrupt_handler(IRQ1, keyboard_callback);
}
