#include "../cpu/isr.h"
#include "../drivers/screen.h"
#include "kernel.h"
#include "../libc/string.h"
#include "../libc/mem.h"


int arg = 0; //Is an argument being taken?
int argt = 0; //Which Command Is taking the argument?
void main() {
	clear_screen();
	kprint("DripOS 0.001\n"); //Version
    isr_install();
    irq_install();

    kprint("Type help for commands\n"
        "Type end to halt the CPU\n> ");
}

void user_input(char *input) {
	if (arg == 1) {
		if(argt == 0) {
			if (strcmp(input, "end") == 0) {
				kprint("Stops the cpu\n");
			} else if (strcmp(input, "page") == 0) {
				kprint("Points to some free memory\n");
			} else if (strcmp(input, "help") == 0) {
				kprint("Lists commands\n");
			} else if (strcmp(input, "chelp") == 0) {
				kprint("Tells about any command\n");
			} else {
				kprint("Unknown command: ");
				kprint(input);
				kprint("\n");
			}
		}
		arg = 0;
	} else if (strcmp(input, "end") == 0) {
        kprint("Stopping the CPU. Bye!\n");
        asm volatile("hlt");
    } else if (strcmp(input, "page") == 0) {
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
    } else if (strcmp(input, "help") == 0) {
		kprint("Commands: page, chelp, help, end\n");
	} else if (strcmp(input, "chelp") == 0) {
		arg = 1;
		argt = 0;
		kprint("On the next prompt, enter the command you want to know about\n");
	} else {
		kprint("Unknown command: ");
		kprint(input);
	}
    kprint("\n> ");
}

