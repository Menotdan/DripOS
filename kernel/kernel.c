//0xEFFFFF
asm(".pushsection .text._start\r\njmp kmain\r\n.popsection\r\n");

#include <stdio.h>
#include <serial.h>
#include <libc.h>
#include "../multiboot.h"
#include "../cpu/isr.h"
#include "../drivers/screen.h"
#include "../drivers/sound.h"
#include "kernel.h"
#include "../libc/string.h"
#include "../libc/mem.h"
#include "../cpu/timer.h"
#include "terminal.h"
//codes
int prevtick = 0;
int login = 1;
int passin = 0;
int state = 0;
int uinlen = 0;
int prompttype = 0;
int stdinpass = 0;
int loaded = 0;
uint32_t lowerMemSize;
uint32_t upperMemSize;
uint32_t largestUseableMem = 0;
uint32_t memAddr = 0;
multiboot_memory_map_t* mmap;
void kmain(multiboot_info_t* mbd, unsigned int endOfCode) {
	char *key_buffer;
	init_serial();
	if (mbd->flags & MULTIBOOT_INFO_MEMORY)
    {
		lowerMemSize = (uint32_t)mbd->mem_lower;
		upperMemSize = (uint32_t)mbd->mem_upper;
    }

    if (mbd->flags & MULTIBOOT_INFO_MEM_MAP)
    {
        for (mmap = (struct multiboot_mmap_entry*)mbd->mmap_addr; (uint32_t)mmap < (mbd->mmap_addr + mbd->mmap_length); mmap = (struct multiboot_mmap_entry*)((uint32_t)mmap + mmap->size + sizeof(mmap->size)))
        {
			uint32_t addrH = mmap->addr_high;
            uint32_t addrL = mmap->addr_low;
            uint32_t lenH = mmap->len_high;
            uint32_t lenL = mmap->len_low;
			uint8_t mType = mmap->type;
			char temp[26];
			int_to_ascii(addrH, temp);
			sprint("ADDR_HIGH: ");
			sprint(temp);
			int_to_ascii(addrL, temp);
			sprint(", ADDR_LOW: ");
			sprint(temp);
			sprint("\n");
			int_to_ascii(lenH, temp);
			sprint("LEN_HIGH: ");
			sprint(temp);
			int_to_ascii(lenL, temp);
			sprint(", LEN_LOW: ");
			sprint(temp);
			int_to_ascii(mType, temp);
			sprint(", MEM_TYPE: ");
			sprint(temp);
			sprint("\n");
			if (mType == 1) {
				if (lenL > largestUseableMem) {
					largestUseableMem = abs(lenL - abs(endOfCode-addrL));
					memAddr = abs(addrL + abs(endOfCode-addrL));
					sprint("\n\nMemory base: ");
					sprint_uint(abs(addrL + abs(endOfCode-addrL)));
					sprint("\nMemory length: ");
					sprint_uint(abs(lenL - abs(endOfCode-addrL)));
					sprint("\nEnd of code: ");
					sprint_uint(endOfCode);
					sprint("\n");
				}
			}
        }
		set_addr(memAddr, largestUseableMem);
		uint32_t f = 0;
		
		key_buffer = (char *)kmalloc(0x2000);
		sprint("\nKey buffer address: ");
		sprint_uint(key_buffer);
		//memory_set(key_buffer, 0, 0x2000);
		sprint("\n");
    }
	sprint("\n[DripOS]: Memory initialized successfully\n");
	isr_install();
	sprint("[DripOS]: ISR Enabled\n");
	irq_install();
	sprint("[DripOS]: IRQ Enabled\n");
	sprint("[DripOS]: Interrupts enabled\n");
	init_timer(1);
	sprint("[DripOS]: Timer enabled\n");
	//new_scan();
	sprintd("Scanning for drives...");
	drive_scan();
	sprintd("Drive scan finished");
	sprintd("Running memory test...");
	char *testOnStart = (char *)kmalloc(0x1000);
	sprintd("Memory allocated for test...");
	strcpy(*testOnStart, "testing testing 123456789123456789123456789123456789123456789123456789123456789123456789123456789123456789\n\0");
	sprintd("Moved data...");
	sprintd("Data:");
	sprint(*testOnStart);
	sprintd("Freeing memory...");
	free(testOnStart, 0x1000);
	sprintd("Memory freed, test done.");
	//wait(1000);
	sprintd("Clearing screen...");
	clear_screen();
	empty_sector();
	//ata_pio28(ata_controler, 1, ata_drive, 0x1);
	prevtick = tick;
	sprintd("Drawing logo");
	logoDraw();
	sprintd("Waiting...");
	wait(100);
	clear_screen();
	stdin_init();
	sprintd("Standard input initialized");
	sprintd("Clearing keyboard buffer bcuz some weird bug");
	key_handler(28, false);
	clear_screen();
	key_handler(28, true);
	sprintd("Keyboard buffer clean");
	kprint("DripOS 0.0020\n"); //Version
	sprintd("DripOS 0.0020 loaded"); //Version
	sprintd("Checking for crashes");
	check_crash();
	kprint("Type help for commands\nType shutdown to shutdown\n\n");
	kprint("Memory available: ");
	char test[25];
	int_to_ascii(memoryRemaining, test);
	kprint(test);
	kprint(" bytes\n");
	kprint("drip@DripOS> ");
	//user_input("testMem");
	//play_sound(500, 100);
	//play_sound(300, 100);
	//clear_screen();
	sprintd("Entering multitask/system management loop");
	loaded = 1;
	after_load();
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
        uint32_t page = kmalloc(0x1000);
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

void after_load() {
	while (1 == 1) {
		manage_sys();
	}
}