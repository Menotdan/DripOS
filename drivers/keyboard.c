/*
    keyboard.c
    Copyright Menotdan and cfenollosa 2018-2019

    Keyboard driver

    MIT License
*/

#include <drivers/keyboard.h>

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <function.h>
#include <drivers/screen.h>
#include <drivers/stdin.h>
#include "../../kernel/kernel.h"
#include "../../cpu/ports.h"
#include "../../cpu/isr.h"


// static void keyboard_callback(registers_t *regs) {
//     /* The PIC leaves us the scancode in port 0x60 */
//     uint8_t scancode = port_byte_in(0x60);
	
// 	//kprint_int(scancode);

// 	bool iskeyup = false;
// 	if (scancode >= KEYUPOFFSET) {
// 		iskeyup = true;
// 		scancode -= KEYUPOFFSET;
// 	}
// 	//kprint("\nKey Buffer before: ");
// 	//kprint(key_buffer);
// 	key_handler(scancode, iskeyup);
// 	//kprint("\nKey Buffer after: ");
// 	//kprint(key_buffer);
//     UNUSED(regs);
// }

// void init_keyboard() {
// 	register_interrupt_handler(IRQ1, keyboard_callback);
// }