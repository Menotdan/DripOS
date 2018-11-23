#include "../cpu/isr.h"
#include "../drivers/screen.h"
#include "../drivers/sound.h"
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
void kmain() {
	isr_install();
	irq_install();
	init_timer(1);
	clear_screen();
	prevtick = tick;
//	drip();
	logoDraw();
	beep();
	wait(100);
	clear_screen();
	kprint("\nDripOS 0.001\n"); //Version
    kprint("Type help for commands\nType shutdown to shutdown\n> ");
}

void beep() {
	play_sound(1050, 10);
	play_sound(1150, 20);
	play_sound(900, 15);
	play_sound(1200, 15);
}

void user_input(char *input) {
	if (arg == 1) {
		if(argt == 1) {
				if(strcmp(input, "password") == 0) {
					shutdown();
				} else if(strcmp(input, "cancel") == 0) {
					arg = 0;
					argt = 0;
					prompttype = 0;
				} else {
					kprint("Password incorrect! Type cancel to cancel");
				}
			}
	} else if (strcmp(input, "shutdown") == 0) {
		arg = 1;
		argt = 1;
		prompttype = 1;
    } else if (strcmp(input, "panic") == 0) {
		panic();
    } else if (strcmp(input, "nmem") == 0) {
        memory();
    } else if (strcmp(input, "help") == 0) {
		kprint("Commands: nmem, help, shutdown, panic\n");
	} else {
		kprint("Unknown command: ");
		kprint(input);
	}
	if(state == 0) {
		if(argt != 1) {
			kprint("\n");
			kprint("drip@DripOS> ");
		} else {
			kprint("\nPassword: ");
		}
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
        u32 phys_addr;
        u32 page = kmalloc(1000, 1, &phys_addr);
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
