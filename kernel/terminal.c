#include "kernel.h"
#include "../libc/string.h"
#include "../cpu/soundManager.h"
#include "../cpu/timer.h"
#include "../drivers/sound.h"
#include <stdint.h>

int arg = 0; //Is an argument being taken?
int argt = 0; //Which Command Is taking the argument?

void execute_command(char *input) {
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
		kprint("Commands: nmem, help, shutdown, panic, print, clear\n");
	} else if (strcmp(input, "clear") == 0){
		clear_screen();
	} else if (match("print", input) == -2) {
		kprint("Not enough args!");
	} else if ((match(input, "print") + 1) == 5) {
		kprint(afterSpace(input));
	} else if (match("tone", input) == -2) {
		kprint("Not enough args!");
		wait(30);
	} else if ((match(input, "tone") + 1) == 4) {
		char test[20];
		int_to_ascii(atoi(afterSpace(input)), test);
		kprint(test);
		p_tone(atoi(afterSpace(input)), 100);
	} else {
		kprint("Unknown command: ");
		kprint(input);
		p_tone(100, 5);
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

void p_tone(uint32_t soundin, int len) {
	play(soundin);
	lSnd = tick;
	pSnd = len;
}

