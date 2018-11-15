#include "../cpu/isr.h"
#include "../drivers/screen.h"
#include "../kernel/kernel.h"
#include "../libc/string.h"
#include "../libc/mem.h"
#include "../cpu/timer.h"

#include "terminal.h"

void terminal_init() {
  kprint("Type help for commands\nType shutdown to shutdown\n> ");
}

void terminal_input(char *input) {
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
        memory_print();
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
