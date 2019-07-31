//0xEFFFFF
asm(".pushsection .text.start\r\n" \
         "jmp main\r\n" \
         ".popsection\r\n"
         );

#include "../cpu/isr.h"
#include "../drivers/screen.h"
#include "../drivers/sound.h"
#include "kernel.h"
#include "../libc/string.h"
#include "../libc/mem.h"
#include "../cpu/timer.h"
#include "terminal.h"
#include "../drivers/stdin.h"
#include "../libc/stdio.h"
#include "../fs/hdd.h"
#include "../fs/hddw.h"
//codes
int prevtick = 0;
int login = 1;
int passin = 0;
int state = 0;
int uinlen = 0;
int prompttype = 0;
int stdinpass = 0;
void main() {
	isr_install();
	irq_install();
	init_timer(1);
	clear_screen();
	empty_sector();
	//ata_pio28(ata_controler, 1, ata_drive, 0x1);
	prevtick = tick;
	logoDraw();
	wait(100);
	clear_screen();
	kprint("DripOS 0.0012\n"); //Version
	check_crash();
    kprint("Type help for commands\nType shutdown to shutdown\n> ");
	stdin_init();
	backspace(key_buffer);
	play_sound(500, 100);
	play_sound(300, 100);
}

void user_input(char *input) {
	if (stdinpass == 0){
		execute_command(input);
	}
	else {
		stdinpass = 0;
		stdin_call(input);
	}
}

void halt() {
	asm volatile("hlt");
}

void shutdown() {
	kprint("System shutdown");
	state = 1;
}

void panic() {
	state = 2;
}

int getstate() {
	return state;
}

void memory() {
	/* Lesson 22: Code to test kmalloc, the rest is unchanged */
        uint32_t phys_addr;
        uint32_t page = kmalloc(1000, 1, &phys_addr);
        char page_str[16] = "";
        hex_to_ascii(page, page_str);
        char phys_str[16] = "";
        hex_to_ascii(phys_addr, phys_str);
        kprint("Page: ");
        kprint(page_str);
        kprint(", physical address: ");
        kprint(phys_str);
        kprint("\n");
}

void check_crash() {
	//0x7263
	read(128);
	if (readOut[0] == 0x7263) {
		kprint("NOTICE: Last time your OS stopped, it was from a crash.\n");
	}
	writeIn[0] = 0x0000;
	writeFromBuffer(128);
}
