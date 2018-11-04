#include "../cpu/isr.h"
#include "../drivers/screen.h"
#include "kernel.h"
#include "../libc/string.h"
#include "../libc/mem.h"
#include "../cpu/timer.h"
<<<<<<< HEAD
//codes
=======
>>>>>>> ab5c5fe1324ce987a89224738614a54b559d9946
int prevtick = 0;
int arg = 0; //Is an argument being taken?
int argt = 0; //Which Command Is taking the argument?
int login = 1;
int passin = 0;
<<<<<<< HEAD
int state = 0;
int uinlen = 0;
void main() {
	isr_install();
	irq_install();
=======
void main() {
	isr_install();
    irq_install();
>>>>>>> ab5c5fe1324ce987a89224738614a54b559d9946
	init_timer(50);
	clear_screen();
	prevtick = tick;
	drip();
	wait(120);
	clear_screen();
	kprint("DripOS 0.001\n"); //Version
<<<<<<< HEAD
    kprint("Type help for commands\nType end to halt the CPU\n> ");
=======
    kprint("Type help for commands\n"
        "Type end to halt the CPU\n> ");
>>>>>>> ab5c5fe1324ce987a89224738614a54b559d9946
}

void user_input(char *input) {
	if (arg == 1) {
<<<<<<< HEAD
		if(argt == 1) {
				if(strcmp(input, "password") == 0) {
					shutdown();
					//arg = 0;
					//argt = 0;
=======
		if(argt == 0) {
			if (strcmp(input, "end") == 0) {
				kprint("Stops the cpu\n");
				arg = 0;
			} else if (strcmp(input, "page") == 0) {
				kprint("Points to some free memory\n");
				arg = 0;
			} else if (strcmp(input, "help") == 0) {
				kprint("Lists commands\n");
				arg = 0;
			} else if (strcmp(input, "chelp") == 0) {
				kprint("Tells about any command\n");
				arg = 0;
			} else {
				kprint("Unknown command: ");
				kprint(input);
				kprint("\n");
				arg = 0;
			}
		} else if(argt == 1) {
				if(strcmp(input, "login") == 0) {
					halt();
					arg = 0;
					argt = 0;
>>>>>>> ab5c5fe1324ce987a89224738614a54b559d9946
				} else if(strcmp(input, "cancel") == 0) {
					arg = 0;
					argt = 0;
				} else {
					kprint("Password incorrect! Type cancel to cancel");
				}
			}
<<<<<<< HEAD
	} else if (strcmp(input, "shutdown") == 0) {
		arg = 1;
		argt = 1;
    } else if (strcmp(input, "panic") == 0) {
		panic();
    } else if (strcmp(input, "nmem") == 0) {
        memory();
    } else if (strcmp(input, "help") == 0) {
		kprint("Commands: nmem, help, shutdown, panic\n");
=======
	} else if (strcmp(input, "end") == 0) {
		arg = 1;
		argt = 1;
		kprint("Password: ");
    } else if (strcmp(input, "page") == 0) {
        memory();
    } else if (strcmp(input, "help") == 0) {
		kprint("Commands: page, chelp, help, end\n");
	} else if (strcmp(input, "chelp") == 0) {
		arg = 1;
		argt = 0;
		kprint("On the next prompt, enter the command you want to know about\n");
>>>>>>> ab5c5fe1324ce987a89224738614a54b559d9946
	} else {
		kprint("Unknown command: ");
		kprint(input);
	}
<<<<<<< HEAD
	if(state == 0) {
		if(argt != 1) {
			kprint("\n");
			kprint("drip@DripOS> ");
		} else {
			kprint("\nPassword: ");
		}
=======
	if(argt != 1) {
		kprint("\n");
		kprint("drip@DripOS> ");
	} else {
		kprint("\nPassword: ");
>>>>>>> ab5c5fe1324ce987a89224738614a54b559d9946
	}
}

void halt() {
<<<<<<< HEAD
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

=======
	kprint("Stopping the CPU. Bye!\n");
	asm volatile("hlt");
}

>>>>>>> ab5c5fe1324ce987a89224738614a54b559d9946
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
<<<<<<< HEAD
}
=======
}
>>>>>>> ab5c5fe1324ce987a89224738614a54b559d9946
