//0xEFFFFF
asm(".pushsection .text._start\r\njmp kmain\r\n.popsection\r\n");

#include "../multiboot.h"
#include "../cpu/isr.h"
#include "../drivers/screen.h"
#include "../drivers/sound.h"
#include "kernel.h"
#include "../libc/string.h"
#include "../libc/mem.h"
#include "../cpu/timer.h"
#include "terminal.h"
#include "../drivers/stdin.h"
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
int loaded = 0;
uint32_t lowerMemSize;
uint32_t upperMemSize;
struct multiboot_mmap_entry_t* mmap;
void kmain(multiboot_info_t* mbd) {
	if (mbd->flags & MULTIBOOT_INFO_MEMORY)
    {
		//char temp[25];
		//int_to_ascii((uint32_t)mbd->mem_lower, temp);
		lowerMemSize = (uint32_t)mbd->mem_lower;
		//kprint("mem_lower = ");
		//kprint(temp);
		//kprint("KB, ");
		//int_to_ascii((uint32_t)mbd->mem_upper, temp);
		upperMemSize = (uint32_t)mbd->mem_upper;
		//kprint("mem_upper = ");
		//kprint(temp);
		//kprint("KB\n");
    }

    // if (mbd->flags & MULTIBOOT_INFO_MEM_MAP)
    // {
    //     vga_printf("mmap_addr = 0x%x, mmap_length = 0x%x\n",
    //             (uint32_t)mbd->mmap_addr, (uint32_t)mbd->mmap_length);

    //     for (mmap = (struct multiboot_mmap_entry*)mbd->mmap_addr;
    //             (uint32_t)mmap < (mbd->mmap_addr + mbd->mmap_length);
    //             mmap = (struct multiboot_mmap_entry*)((uint32_t)mmap
    //                 + mmap->size + sizeof(mmap->size)))
    //     {
    //         vga_printf("base_addr_high = 0x%x, base_addr_low = 0x%x, "
    //                 "length_high = 0x%x, length_low = 0x%x, type = 0x%x\n",
    //                 mmap->addr >> 32,
    //                 mmap->addr & 0xFFFFFFFF,
    //                 mmap->len >> 32,
    //                 mmap->len & 0xFFFFFFFF,
    //                 (uint32_t)mmap->type);
    //     }
    // }


	isr_install();
	irq_install();
	init_timer(1);
	drive_scan();
	//wait(400);
	clear_screen();
	empty_sector();
	ata_pio28(ata_controler, 1, ata_drive, 0x1);
	prevtick = tick;
	logoDraw();
	wait(100);
	clear_screen();
	kprint("DripOS 0.0020\n"); //Version
	check_crash();
	kprint("Type help for commands\nType shutdown to shutdown\n\n");
	kprint("Memory available: ");
	char test[25];
	int_to_ascii(upperMemSize, test);
	kprint(test);
	kprint("KB\n");
	kprint("drip@DripOS> ");
	stdin_init();
	backspace(key_buffer);
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
        uint32_t page = kmalloc(1000, 1, &phys_addr);
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