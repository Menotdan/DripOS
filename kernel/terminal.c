#include "kernel.h"
#include "../libc/string.h"
#include "../cpu/soundManager.h"
#include "../cpu/timer.h"
#include "../drivers/sound.h"
#include "../drivers/time.h"
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
					kprint("Password incorrect! Enter cancel to cancel.");
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
		kprint("Commands: nmem, help, shutdown, panic, print, clear, bgtask, bgoff, time\n");
	} else if (strcmp(input, "clear") == 0){
		clear_screen();
	} else if (match("print", input) == -2) {
		kprint("Not enough args!");
	} else if ((match(input, "print") + 2) == 6) {
		kprint(afterSpace(input));
	} else if (match("tone", input) == -2) {
		kprint("Not enough args!");
		wait(30);
	} else if ((match(input, "tone") + 1) == 4) {
		char test[20];
		int_to_ascii(atoi(afterSpace(input)), test);
		kprint(test);
		p_tone(atoi(afterSpace(input)), 100);
	} else if (strcmp(input, "bgtask") == 0) {
		kprint("Background task started!");
		task = 1;
	} else if (strcmp(input, "bgoff") == 0) {
		kprint("Background task stopped!");
		task = 0;
	} else if (strcmp(input, "time") == 0) {
		read_rtc();
		kprint_int(year);
		kprint("/");
		kprint_int(month);
		kprint("/");
		kprint_int(day);
		if (hour - 5 < 0) {
			kprint(" ");
			kprint_int(24 + (hour - 5));
		} else if(hour - 5 <= 10) {
			kprint(" 0");
			kprint_int(hour - 5);
		} else {
			kprint(" ");
			kprint_int(hour - 5);
		}
		if (minute > 9) {
			kprint(":");
			kprint_int(minute);
		} else {
			kprint(":0");
			kprint_int(minute);
		}
		if (second > 9) {
			kprint(":");
			kprint_int(second);
		} else {
			kprint(":0");
			kprint_int(second);
		}
	} else if (strcmp(input, "pgw") == 0) {
		kprint("wow");
	} else if (strcmp(input, "read") == 0) {
		read_disk();
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

void read_disk() {
	uint32_t sectornum = 0;
	uint8_t* sector = 0;
	sectornum = 0;
 
	kprint ("\nSector 0 contents:\n\n");
 
	//! read sector from disk
	sector = flpydsk_read_sector ( sectornum );
 
	//! display sector
	if (sector!=0) {
 
		int i = 0;
		for ( int c = 0; c < 4; c++ ) {
 
			for (int j = 0; j < 128; j++)
				kprint ("%x", sector[ i + j ]);
			i += 128;

		}
	}
	else
		kprint ("\n*** Error reading sector from disk");
 
	kprint ("\nDone.");
}