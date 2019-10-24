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
#include "../fs/fat32.h"
#include "debug.h"
//codes
int prevtick = 0;
int login = 1;
int passin = 0;
int state = 0;
uint32_t uinlen = 0;
uint32_t position = 0;
int prompttype = 0;
int stdinpass = 0;
int loaded = 0;
uint32_t lowerMemSize;
uint32_t upperMemSize;
uint32_t largestUseableMem = 0;
uint32_t memAddr = 0;
multiboot_memory_map_t* mmap;
char key_buffer[2000];
char key_buffer_up[2000];
char key_buffer_down[2000];
//int32_t path_clusters[50]; // Cluster pointers so the kernel knows what directory it is in

void after_load() {
	while (1 == 1) {
		uint32_t l = strlen(key_buffer);
		if (key_buffer[l] == 3 || key_buffer[l-1] == 3) {
			backspace(key_buffer);
			user_input(key_buffer);
			for (uint32_t i = 0; i < 2000; i++) {
				key_buffer[i] = 0;
			}
			sprintd(key_buffer);
			uinlen = 0;
			position = 0;
		}
	}
}

void Log(char *message, int type) {
	if (type == 1) { // Info
		kprint("\n[");
		kprint_color("INFO", 0x01);
		kprint("]: ");
		kprint_color(message, 0x09);
	}
}

void kmain(multiboot_info_t* mbd, unsigned int endOfCode) {
	// Read memory map
	//breakA();
	init_serial();
	//breakA();
	if (mbd->flags & MULTIBOOT_INFO_MEMORY)
    {
		lowerMemSize = (uint32_t)mbd->mem_lower;
		upperMemSize = (uint32_t)mbd->mem_upper;
    }
	breakA();
    if (mbd->flags & MULTIBOOT_INFO_MEM_MAP)
    {
        for (mmap = (struct multiboot_mmap_entry*)mbd->mmap_addr; (uint32_t)mmap < (mbd->mmap_addr + mbd->mmap_length); mmap = (struct multiboot_mmap_entry*)((uint32_t)mmap + mmap->size + sizeof(mmap->size)))
        {
			uint32_t addrH = mmap->addr_high;
            uint32_t addrL = mmap->addr_low;
            uint32_t lenH = mmap->len_high;
            uint32_t lenL = mmap->len_low;
			uint8_t mType = mmap->type;
			kprint("\n\n");
			kprint("ADDR_HIGH: ");
			kprint_uint(addrH);
			kprint(", ADDR_LOW: ");
			kprint_uint(addrL);
			kprint("\n");
			kprint("LEN_HIGH: ");
			kprint_uint(lenH);
			kprint(", LEN_LOW: ");
			kprint_uint(lenL);
			kprint(", MEM_TYPE: ");
			kprint_uint(mType);
			if (mType == 1) {
				if (lenL > largestUseableMem) {
					largestUseableMem = abs(lenL - abs(endOfCode-addrL));
					memAddr = abs(addrL + abs(endOfCode-addrL));
				}
			}
        }
		kprint("\nEnd of code: ");
		kprint_uint(endOfCode);
		kprint("\nCalculated address: ");
		kprint_uint(memAddr);
		set_addr(memAddr, largestUseableMem);
    }
	//breakA();
	clear_screen();
	//breakA();
	// Initialize everything with a startup log
	Log("Loaded memory", 1);
	//breakA();
	isr_install();
	//breakA();
	Log("ISR Enabled", 1);
	//breakA();
	irq_install();
	//breakA();
	Log("Interrupts Enabled", 1);
	//breakA();
	init_timer(1);
	//breakA();
	Log("Timer enabled", 1);
	//breakA();

	//breakA();
	Log("Scanning for drives", 1);
	//breakA();
	drive_scan();
	//breakA();
	Log("Drive scan done", 1);
	//breakA();

	Log("Starting the HDD driver", 1);
	//breakA();
	init_hddw();
	//breakA();
	Log("Done", 1);
	//breakA();

	// Log("Formatting drive...", 1);
	// //user_input("select 1");
	// format();
	// Log("Formatted", 1);
	// init_fat();
	// Log("Initialized", 1);

	// dir_entry_t *new_file_created = kmalloc(sizeof(dir_entry_t));
	// uint32_t *data_to_write = kmalloc(512);
	// uint32_t *data_to_read = kmalloc(512);
	// *data_to_write = 123456789;
	// new_file("test", "txt", (uint32_t)&new_file_created, 512);
	// write_data_to_entry(new_file_created, data_to_write, 512);
	// read_data_from_entry(new_file_created, data_to_read);
	// sprint("\nData read from ");
	// char filename[13];
	// fat_str(new_file_created->name, new_file_created->ext, filename);
	// sprint(filename);
	// sprint(": ");
	// sprint_uint(*data_to_read);
	// sprint("\n");
	// free(data_to_write, 512);
	// free(data_to_read, 512);
	// free(new_file_created, sizeof(dir_entry_t));

	// Log("Done", 1);

	Log("Testing mem", 1);
	uint32_t *testOnStart = (uint32_t *)kmalloc(0x1000);
	*testOnStart = 33;
	free(testOnStart, 0x1000);
	Log("Test done", 1);

	Log("Clearing screen...", 1);
	wait(50);
	clear_screen();
	prevtick = tick;
	logoDraw();
	play_sound(300, 50);
	play_sound(500, 50);
	clear_screen();
	stdin_init();
	kprint("DripOS 0.0020\n"); //Version
	sprintd("DripOS 0.0020 loaded"); //Version
	//check_crash();
	kprint("Type help for commands\nType shutdown to shutdown\n\n");
	kprint("Memory available: ");
	char test[25];
	int_to_ascii(memoryRemaining, test);
	kprint(test);
	kprint(" bytes\n");
	kprint("drip@DripOS> ");
	sprintd("Entering multitask/system management loop");
	loaded = 1;
	after_load();
}

void user_input(char input[]) {
	//sprintd(input);
	if (stdinpass == 0){
		execute_command(input);
	}
	else {
		stdinpass = 0;
		//stdin_call(input);
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

// void check_crash() {
// 	//0x7263
// 	read(128, 0);
// 	if (readOut[0] == 0x7263) {
// 		kprint("NOTICE: Last time your OS stopped, it was from a crash.\n");
// 	}
// 	writeIn[0] = 0x0000;
// 	write(128);
// }

