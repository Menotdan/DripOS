#include "kernel.h"
#include "../libc/string.h"
#include "../cpu/soundManager.h"
#include "../cpu/timer.h"
#include "../drivers/sound.h"
#include "../drivers/time.h"
#include "../fs/hdd.h"
#include "../libc/stdio.h"
#include <stdint.h>

int arg = 0; //Is an argument being taken?
int argt = 0; //Which Command Is taking the argument?

void execute_command(char *input) {
    if (strcmp(input, "shutdown") == 0) {
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
	} else if (strcmp(input, "read") == 0) {
		read_disk();
	} else {
		kprint("Unknown command: ");
		kprint(input);
		p_tone(100, 5);
	}
	kprint("\n");
	kprint("drip@DripOS> ");
}

void p_tone(uint32_t soundin, int len) {
	play(soundin);
	lSnd = tick;
	pSnd = len;
}

void read_disk() {
	uint32_t sectornum;
	uint16_t *sector;
	uint16_t nom;
	sectornum = 0;
	char str1[32];
	char str2[32];
	kprint ("\nSector ");
	kprint_int(sectornum);
	kprint(" contents:\n\n");
 
	//! read sector from disk
	ata_pio28(ata_controler, 1, ata_drive, 0x0);
	for (int l = 0; l<256; l++) {
		//hex_to_ascii(sector[l] & 0xff, str1);
		//hex_to_ascii((sector[l] >> 8), str2);
		//kprint(str2);
		//kprint(" ");
		//kprint(str1);
		//kprint(" ");
		hex_to_ascii(sector[l], str1);
		//kprint(str1);
		//kprint(" ");
		for (int i = 0; i<32; i++) {
			str1[i] = 0;
			str2[i] = 0;
		}
	}
	kprint_int(sizeof(sector));
}