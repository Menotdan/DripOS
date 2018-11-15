#include "../cpu/isr.h"
#include "../drivers/screen.h"
#include "kernel.h"
#include "../libc/string.h"
#include "../libc/mem.h"
#include "../cpu/timer.h"
//codes
int prevtick = 0;
int arg = 0; //Is an argument being taken?
int argt = 0; //Which Command Is taking the argument?
int login = 1;
int passin = 0;
int state = 0;
int uinlen = 0;
int prompttype = 0;
void main() {
	isr_install();
	irq_install();
	init_timer(1);
	clear_screen();
	prevtick = tick;
	drip();
	wait(100);
	clear_screen();
	kprint("DripOS 0.001\n"); //Version
	terminal_init();
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
