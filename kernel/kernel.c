//0xEFFFFF
asm(".pushsection .text._start\r\njmp kmain\r\n.popsection\r\n");

#include "kernel.h"
#include <stdio.h>
#include <serial.h>
#include <libc.h>
#include "../multiboot.h"
#include "../cpu/isr.h"
#include "../drivers/screen.h"
#include "../drivers/sound.h"
#include <string.h>
#include "../libc/mem.h"
#include "../mem/pmm.h"
#include "../mem/vmm.h"
#include "../cpu/timer.h"
#include "../drivers/time.h"
#include "terminal.h"
#include "debug.h"
#include "../fs/dripfs.h"
#include "../cpu/task.h"
#include "../drivers/ps2.h"
#include "../drivers/vesa.h"

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
uint64_t mem_size;
uint64_t total_mem = 0;
uint64_t largest_usable_mem = 0;
uint64_t mem_addr = 0;
multiboot_memory_map_t* mmap;
char key_buffer[2000];
char key_buffer_up[2000];
char key_buffer_down[2000];
char cpu_name[33];
uint8_t *vidmem;
uint16_t width;
uint16_t height;
uint32_t bbp; // Bytes, not bits
uint32_t extra_bits;
uint32_t bpl; // Bytes per line
uint8_t red_byte;
uint8_t blue_byte;
uint8_t green_byte;
uint32_t char_w;
uint32_t char_h;

void after_load() {
	while (1 == 1) {
		
	}
}

void Log(char *message, int type) {
	if (type == 1) { // Info
		kprint("\n[");
		kprint_color("INFO", color_from_rgb(0,0,255), color_from_rgb(0,0,0));
		kprint("]: ");
		kprint_color(message, color_from_rgb(0,255,255), color_from_rgb(0,0,0));
	} else if (type == 2) { // Warn
		kprint("\n[");
		kprint_color("WARN", color_from_rgb(255,255,0), color_from_rgb(0,0,0));
		kprint("]: ");
		kprint_color(message, color_from_rgb(155, 155, 0), color_from_rgb(0,0,0));
	} else if (type == 3) { // Good
		kprint("\n[");
		kprint_color("SUCCESS", color_from_rgb(0,255,0), color_from_rgb(0,0,0));
		kprint("]: ");
		kprint_color(message, color_from_rgb(0,155,0), color_from_rgb(0,0,0));
	}
}

void interrupt_test() {
	asm("int $32");
}

void get_cpu_name(char *str) {
    asm volatile (
		"  mov      $0x80000002, %%eax\n"
		"  cpuid\n"
		"  stosl\n"
		"  mov      %%ebx, %%eax\n"
		"  stosl\n"
		"  mov      %%ecx, %%eax\n"
		"  stosl\n"
		"  mov      %%edx, %%eax\n"
		"  stosl\n"
		"  mov      $0x80000003, %%eax\n"
		"  cpuid\n"
		"  stosl\n"
		"  mov      %%ebx, %%eax\n"
		"  stosl\n"
		"  mov      %%ecx, %%eax\n"
		"  stosl\n"
		"  mov      %%edx, %%eax\n"
		"  stosl\n"
		"  mov      $0x80000004, %%eax\n"
		"  cpuid\n"
		"  stosl\n"
		"  mov      %%ebx, %%eax\n"
		"  stosl\n"

        :
        : "D" (str)
        : "rax", "rbx", "rcx", "rdx"
    );
}

void kmain(multiboot_info_t* mbd, uint32_t end_of_code) {
	// Read memory map
	init_serial();
	sprint_uint64(0xffffffffffffffff);
	sprintf("\nMultiboot header: %lu", (uint64_t) mbd);
	sprintf("\nMemory lower: %lu", mbd->mem_lower);
	sprintf("\nMemory upper: %lu", mbd->mem_upper);
	sprintf("\nFramebuffer height: %u", mbd->framebuffer_height);
	sprintf("\nKernel end: %lx", end_of_code);
	sprintf("\n\nVESA info:\n  Colors: %u\n  Red pos: %u\n  Green pos: %u\n  Blue pos: %u", (uint32_t) mbd->framebuffer_palette_num_colors, (uint32_t) mbd->framebuffer_red_field_position, (uint32_t) mbd->framebuffer_green_field_position, (uint32_t) mbd->framebuffer_blue_field_position);
	sprintf("\n  Framebuffer pitch: %u\n  Framebuffer size: %lu\n  Framebuffer width: %lu", mbd->framebuffer_pitch, (uint64_t) (mbd->framebuffer_height * mbd->framebuffer_pitch), mbd->framebuffer_width);
	sprint("\nInitializing stage 1 paging and reading memory map");
	if (mbd->flags & MULTIBOOT_INFO_MEM_MAP) {
		sprintf("\nMemory map exists. Address: %x", mbd->mmap_addr);
		sprintf("\n Size: %u", mbd->mmap_length);
		configure_mem(mbd);
		uint64_t phys_framebuffer = mbd->framebuffer_addr & ~(0xfff);
		uint64_t framebuffer_size = mbd->framebuffer_height * mbd->framebuffer_pitch;
		uint64_t framebuffer_pages = (framebuffer_size + 0x1000 - 1) / 0x1000;
		// framebuffer_pages += 3;
		sprintf("\nCalculated framebuffer size: %lu", framebuffer_size);
		sprintf("\nCalculated framebuffer pages: %lu", framebuffer_pages);
		vmm_map((void *) phys_framebuffer, (void *) phys_framebuffer, framebuffer_pages, 0);
		sprintf("\nMapped framebuffer.");
	}
	sprint("\nCPU name: ");
	get_cpu_name(cpu_name);
	sprint(cpu_name);
	sprint("\n");
	
	sprint("\nSetting up interrupts, so I can have exception handlers when my paging dies");
	isr_install();
	irq_install();
	
	while (1) {
		asm volatile("hlt");
	}
	setup_screen();
	/* VESA SET? */
	if ((mbd->flags & MULTIBOOT_INFO_VBE_INFO)) {
		// VBE ready
		sprint("\nWidth: ");
		sprint_uint(mbd->framebuffer_width);
		width = mbd->framebuffer_width;
		sprint("\nHeight: ");
		sprint_uint(mbd->framebuffer_height);
		height = mbd->framebuffer_height;
		sprint("\nFramebuffer address: ");
		sprint_uint(mbd->framebuffer_addr);
		sprint("\nColors: ");
		sprint_uint(mbd->framebuffer_palette_num_colors);
		vidmem = (uint8_t *)mbd->framebuffer_addr;
		sprint("\nBPP: ");
		sprint_uint(mbd->framebuffer_bpp);
		sprint("\nBytes per pixel: ");
		sprint_uint(mbd->framebuffer_bpp/8);
		sprint("\nBytes per line: ");
		sprint_uint(mbd->framebuffer_pitch);
		sprint("\nLeftover: ");
		sprint_uint(mbd->framebuffer_bpp%8);
		bbp = mbd->framebuffer_bpp/8;
		extra_bits = mbd->framebuffer_bpp%8;
		sprint("\nPitch: ");
		sprint_uint(mbd->framebuffer_pitch);
		bpl = mbd->framebuffer_pitch;
		red_byte = mbd->framebuffer_red_field_position;
		green_byte = mbd->framebuffer_green_field_position;
		blue_byte = mbd->framebuffer_blue_field_position;
		sprint("\nChar width: ");
		sprint_uint(width/8);
		sprint("\nChar height: ");
		sprint_uint(height/8);
		current_buffer = new_framebuffer(0, 0, width/2, height);
	}

	clear_screen();
	// Initialize everything with a startup log
	Log("Loaded memory", 1);
	isr_install();
	Log("ISR Enabled", 1);
	init_timer(1193);

	Log("Timer enabled", 1);
	Log("Loading PS/2", 1);
	init_ps2();
	Log("PS/2 enabled", 3);
	irq_install();
	Log("Interrupts Enabled", 1);

	Log("Scanning for drives", 1);
	drive_scan();
	Log("Drive scan done", 1);

	Log("Starting the HDD driver", 1);
	init_hdd();
	init_hddw();
	Log("Done", 1);

	//Log("Formatting drive with Drip FS", 1);
	//dfs_format("DripOS", 1, 1);
	//Log("Done!", 3);
	// Log("Formatting drive...", 1);
	// user_input("select 1");
	// format();
	// Log("Formatted", 1);
	// init_fat();
	// Log("Initialized", 1);

	Log("Testing mem", 1);
	uint32_t *testOnStart = (uint32_t *)kmalloc(0x1000);
	*testOnStart = 33;
	if (*testOnStart == 33) {
		Log("Test passed!", 3);
	} else {
		Log("Test failed!", 2);
	}
	Log("Test done", 1);

	free(testOnStart, 0x1000);
	Log("Clearing screen...", 1);
	update_display();
	clear_screen();
	prevtick = tick;
	logo_draw();
	play(300);
	wait(15);
	play(500);
	wait(15);
	play(580);
	wait(30);
	nosound();
	update_display();
	clear_screen();

	kprint("DripOS 0.0030\n"); //Version
	sprintd("DripOS 0.0030 loaded"); //Version

	kprint("Type help for commands\nType shutdown to shutdown\n\n");
	kprint("Memory available: ");
	char test[25];
	int_to_ascii(memory_remaining, test);
	kprint(test);
	kprint(" bytes\n");
	kprint("drip@DripOS> ");
	set_RTC_register(0x4, 13);
	sprint("\nHour: ");
	sprint_uint(get_RTC_register(0x4));
	sprintd("Entering multitask/system management loop");

	initTasking();
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

// ^ this was use in old time for bad thing