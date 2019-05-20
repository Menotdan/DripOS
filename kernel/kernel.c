//0x1000
#include "../cpu/isr.h"
#include "../drivers/screen.h"
#include "kernel.h"
#include "../libc/string.h"
#include "../libc/mem.h"
#include "../cpu/timer.h"
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
	prevtick = tick;
	kprint("Loading DripOS...\n"); //Version
	driveLoop();
}

void driveLoop() {
	uint16_t sectors = 31;
	uint16_t sector = 0;
	uint16_t basesector = 40000;
	uint32_t i = 40031;
	uint16_t code[sectors][256];
	int x = 0;
	while(x==0) {
		read(i);
		for (int p=0; p < 256; p++) {
			if (readOut[p] == 0) {
			} else {
				x = 1;
				//kprint_int(i);
			}
		}
		i++;
	}
	kprint("Found sector!\n");
	kprint("Loading OS into memory...\n");
	for (sector=0; sector<sectors; sector++) {
		read(basesector+sector);
		for (int p=0; p<256; p++) {
			code[sector][p] = readOut[p];
		}
	}
	kprint("Done loading.\n");
	kprint("Attempting to call...\n");
	//asm volatile("call (%0)" : : "r" (&code));
	__builtin__clear_cache(&code[0][0], &code[30][255]);   // don't optimize away stores into the buffer
    void (*fptr)(void) =  (void*)code;                     // casting to void* instead of the actual target type is simpler

    fptr();
}

int getstate() {
	return state;
}
