//0xEFFFFF
asm(".pushsection .text._start\r\njmp kmain\r\n.popsection\r\n");

#include <stdio.h>
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
			kprint("ADDR_HIGH: ");
			kprint(temp);
			int_to_ascii(addrL, temp);
			kprint(", ADDR_LOW: ");
			kprint(temp);
			kprint("\n");
			int_to_ascii(lenH, temp);
			kprint("LEN_HIGH: ");
			kprint(temp);
			int_to_ascii(lenL, temp);
			kprint(", LEN_LOW: ");
			kprint(temp);
			int_to_ascii(mType, temp);
			kprint(", MEM_TYPE: ");
			kprint(temp);
			kprint("\n");
			if (mType == 1) {
				if (lenL > largestUseableMem) {
					largestUseableMem = abs(lenL - abs(endOfCode-addrL));
					memAddr = abs(addrL + abs(endOfCode-addrL));
					kprint("\n\nMemory base: ");
					kprint_uint(abs(addrL + abs(endOfCode-addrL)));
					kprint("\nMemory length: ");
					kprint_uint(abs(lenL - abs(endOfCode-addrL)));
					kprint("\nEnd of code: ");
					kprint_uint(endOfCode);
					kprint("\n");
				}
			}
        }
		set_addr(memAddr, largestUseableMem);
		uint32_t f = 0;
		
		*key_buffer = (char *)kmalloc(0x2000);
		kprint("\nKey buffer address: ");
		kprint_uint(&key_buffer);
		kprint("\n");
    }


	isr_install();
	irq_install();
	init_timer(1);
	//new_scan();
	drive_scan();
	wait(1000);
	clear_screen();
	empty_sector();
	//ata_pio28(ata_controler, 1, ata_drive, 0x1);
	prevtick = tick;
	logoDraw();
	wait(100);
	clear_screen();
	kprint("DripOS 0.0020\n"); //Version
	check_crash();
	kprint("Type help for commands\nType shutdown to shutdown\n\n");
	kprint("Memory available: ");
	char test[25];
	int_to_ascii(memoryRemaining, test);
	kprint(test);
	kprint(" bytes\n");
	kprint("drip@DripOS> ");
	stdin_init();
	//user_input("testMem");
	backspace(*key_buffer);
	//play_sound(500, 100);
	//play_sound(300, 100);
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