#include "kernel.h"
#include "debug.h"
#include "../libc/string.h"
#include "../cpu/soundManager.h"
#include "../cpu/timer.h"
#include "../drivers/sound.h"
#include "../drivers/time.h"
#include "../drivers/screen.h"
#include "../fs/hdd.h"
#include "../fs/hddw.h"
//#include "../fs/fat32.h"
#include "../libc/stdio.h"
#include <stdint.h>
#include "../libc/mem.h"
#include <serial.h>
#include "../cpu/task.h"

int arg = 0; //Is an argument being taken?
int argt = 0; //Which Command Is taking the argument?
uint32_t task2 = 0;
Task bg_task_timer;
void bg_task() {
	while (1)
	{
		char done[24];
		int_to_ascii((int)tick, done);
		kprint_no_move(done, 0, 0);
		//yield();
	}
}

Task bg_task_timer2;
void bg_task2() {
	while (1)
	{
		char done[24];
		int_to_ascii((int)tick, done);
		kprint_no_move(done, 18, 0);
		//yield();
	}
}

void read_disk(uint32_t sector) {
	char str2[32];
	kprint ("\nSector ");
	kprint_int(sector);
	kprint(" contents:\n\n");
 
	//! read sector from disk
	readToBuffer(sector);
	uint8_t *temp = readBuffer;
	for (int l = 0; l<256; l++) {
		uint16_t good = *temp;
		temp++;
		good += (uint16_t) (*temp << 8);
		temp++;
		hex_to_ascii(good, str2);
		kprint(str2);
		kprint(" ");
		for (int i = 0; i<32; i++) {
			str2[i] = 0;
		}
	}
	clear_ata_buffer();
}

void p_tone(uint32_t soundin, int len) {
	play(soundin);
	lSnd = tick;
	pSnd = len;
}

void execute_command(char input[]) {
	char *testy;
	sprintd(input);
	sprintd("Command recieved");
	kprint("\n");
  if (strcmp(input, "shutdown") == 0) {
		shutdown();
  } else if (strcmp(input, "panic") == 0) {
		panic();
  } else if (strcmp(input, "fmem") == 0) {
	  	kprint("No!");
  } else if (strcmp(input, "free") == 0) {
        char temp[25];
		int_to_ascii(memoryRemaining, temp);
		kprint("Memory available: ");
		kprint(temp);
		kprint(" bytes\n");
		int_to_ascii(usedMem, temp);
		kprint("Memory used: ");
		kprint(temp);
		kprint(" bytes");
  } else if (strcmp(input, "uptime") == 0) {
	  	uint32_t tempTick = tick;
	  	uint32_t uptimeSeconds = 0;//tempTick / 100;
		uint32_t uptimeMinutes = 0;
		uint32_t uptimeHours = 0;
		uint32_t uptimeDays = 0;
		while (tempTick >= 100)
		{
			tempTick -= 100;
			uptimeSeconds += 1;
		}

		while (uptimeSeconds >= 60)
		{
			uptimeSeconds -= 60;
			uptimeMinutes += 1;
		}
		

		while (uptimeMinutes >= 60)
		{
			uptimeMinutes -= 60;
			uptimeHours += 1;
		}


		while (uptimeHours >= 24)
		{
			uptimeHours -= 24;
			uptimeDays += 1;
		}


		kprint("Up ");
		kprint_uint(uptimeDays);
		if (uptimeDays == 1) {
			kprint(" day, ");
		} else {
			kprint(" days, ");
		}
		kprint_uint(uptimeHours);
		if (uptimeHours == 1) {
			kprint(" hour, ");
		} else {
			kprint(" hours, ");
		}
		kprint_uint(uptimeMinutes);
		if (uptimeMinutes == 1) {
			kprint(" minute, ");
		} else {
			kprint(" minutes, ");
		}
		kprint_uint(uptimeSeconds);
		if (uptimeSeconds == 1) {
			kprint(" second");
		} else {
			kprint(" seconds");
		}
  } else if (strcmp(input, "help") == 0) {
		kprint("Commands: ps, kill, uptime, scan, testDrive, fmem, help, shutdown, panic, print, clear, bgtask, bgoff, read, drives, select, testMem, free\n");
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
		//if (task == 0) {
			task = createTask(kmalloc(sizeof(Task)), bg_task);
			task2 = createTask(kmalloc(sizeof(Task)), bg_task2);
			kprint("Background task started!");
		//} else {
		//	kprint("Nope");
		//}
	} else if (strcmp(input, "ps") == 0) {
		print_tasks();
	} else if (match("kill", input) == -2) {
		kprint("Not enough args!");
	} else if ((match(input, "kill") + 1) == 4) {
		uint32_t taskToKill = atoi(afterSpace(input));
		int32_t returnVal = kill_task(taskToKill);
		if (returnVal == 0) {
			kprint("\nKilled ");
			kprint_uint(taskToKill);
			kprint("!");
		} else {
			kprint("\nKilling task ");
			kprint_uint(taskToKill);
			kprint(" failed!");
		}
	} else if (strcmp(input, "testMem") == 0) {
		//breakA();
		//asserta(0, "t");
		for (int c = 0; c < 10000; c++) {
			testy = (char *)kmalloc(0x1000);
			kprint_uint((uint32_t)testy);
			kprint("\n");
			*testy = 100;
			//free(testy, 0x1000);
		}
	} else if (strcmp(input, "testMemLess") == 0) {
		uint32_t *test1 = kmalloc(0x1000);
		uint32_t *test2 = kmalloc(0x1000);
		*test2 = 33;
		*test1 = 33;
		sprintd("Data test:");
		sprint_uint(*test1);
		sprint("\n");
		sprintd("Test 2:");
		sprint_uint(*test2);
		char temp[25];
		int_to_ascii(memoryRemaining, temp);
		sprint("\nMemory Remaining: ");
		sprint(temp);
		sprint(" bytes\n");
		free(test1, 0x1000);
		free(test2, 0x1000);
	} else if (strcmp(input, "testMemBlocks") == 0) {
		for (int c = 0; c < 1; c++) {
			sprintd("Allocating 4096 bytes...");
			uint32_t *testy1 = (uint32_t *)kmalloc(0x1000);
			sprintd("Allocating 4096 bytes...");
			uint32_t *testy2 = (uint32_t *)kmalloc(0x1000);
			sprintd("Moving data...");
			*testy1 = 33;
			sprintd("Data:");
			sprint_uint(*testy1);
			*testy2 = 33;
			sprintd("Data:");
			sprint_uint(*testy2);
			sprintd("Freeing memory...");
			free(testy1, 0x1000);
			sprintd("Allocating 4096 bytes...");
			uint32_t *testy3 = (uint32_t *)kmalloc(0x1000);
			sprintd("Moving data...");
			*testy3 = 33;
			sprintd("Data:");
			sprint_uint(*testy3);
			sprintd("Freeing memory...");
			free(testy2, 0x1000);
			sprintd("Freeing memory...");
			free(testy3, 0x1000);
		}
	} else if (strcmp(input, "time") == 0) {
		// read_rtc();
		// kprint_int(year);
		// kprint("/");
		// kprint_int(month);
		// kprint("/");
		// kprint_int(day);
		// if (hour - 5 < 0) {
		// 	kprint(" ");
		// 	kprint_int(24 + (hour - 5));
		// } else if(hour - 5 <= 10) {
		// 	kprint(" 0");
		// 	kprint_int(hour - 5);
		// } else {
		// 	kprint(" ");
		// 	kprint_int(hour - 5);
		// }
		// if (minute > 9) {
		// 	kprint(":");
		// 	kprint_int(minute);
		// } else {
		// 	kprint(":0");
		// 	kprint_int(minute);
		// }
		// if (second > 9) {
		// 	kprint(":");
		// 	kprint_int(second);
		// } else {
		// 	kprint(":0");
		// 	kprint_int(second);
		// }
		kprint("Currently broken. Also you are a hacker for finding this command.");
	} else if (strcmp(input, "scan") == 0) {
		drive_scan();
	} else if (match("testDrive", input) == -2) {
		kprint("Not enough args!");
	} else if ((match(input, "testDrive") + 1) == 9) {
		writeIn[0] = 0x1111;
		write(atoi(afterSpace(input)), 0);
		kprint("Writing 0x1111 to sector ");
		kprint_int(atoi(afterSpace(input)));
		kprint("\n");
	} else if (match("read", input) == -2) {
		kprint("Not enough args!");
	} else if ((match(input, "read") + 1) == 4) {
		read_disk(atoi(afterSpace(input)));
		//kprint(atoi(afterSpace(input)));
	} else if (match("select", input) == -2) {
		kprint("Not enough args!");
	} else if ((match(input, "select") + 1) == 6) {
		uint8_t driveToSet = atoi(afterSpace(input));
		if (driveToSet == 1) {
			if (mp == 0) {
				ata_drive = MASTER_DRIVE;
				ata_controler = PRIMARY_IDE;
				nodrives = 0;
			} else if (mp48 == 0) {
				ata_drive = MASTER_DRIVE_PIO48;
				ata_controler = PRIMARY_IDE;
				ata_pio = 1;
				nodrives = 0;
			} else {
				kprint("That drive is offline!\n");
			}
		} else if (driveToSet == 2) {
			if (ms == 0) {
				ata_drive = SLAVE_DRIVE;
				ata_controler = PRIMARY_IDE;
				nodrives = 0;
			} else if (ms48 == 0) {
				ata_drive = SLAVE_DRIVE_PIO48;
				ata_controler = PRIMARY_IDE;
				ata_pio = 1;
				nodrives = 0;
			} else {
				kprint("That drive is offline!\n");
			}
		} else if (driveToSet == 3) {
			if (sp == 0) {
				ata_drive = MASTER_DRIVE;
				ata_controler = SECONDARY_IDE;
				nodrives = 0;
			} else if (sp48 == 0) {
				ata_drive = MASTER_DRIVE_PIO48;
				ata_controler = SECONDARY_IDE;
				ata_pio = 1;
				nodrives = 0;
			} else {
				kprint("That drive is offline!\n");
			}
		} else if (driveToSet == 4) {
			if (ss == 0) {
				ata_drive = SLAVE_DRIVE;
				ata_controler = SECONDARY_IDE;
				ata_pio = 0;
				nodrives = 0;
			} else if (ss48 == 0) {
				ata_drive = SLAVE_DRIVE_PIO48;
				ata_controler = SECONDARY_IDE;
				ata_pio = 1;
				nodrives = 0;
			} else {
				kprint("That drive is offline!\n");
			}
		} else {
			kprint("Not a valid drive!\n");
		}
		//kprint(atoi(afterSpace(input)));
	} else if (match("copy", input) == -2) {
		kprint("Not enough args!");
	} else if ((match(input, "copy") + 1) == 4) {
		copy_sector(0, atoi(afterSpace(input)));
	} else if (match("clearS", input) == -2) {
		kprint("Not enough args!");
	} else if ((match(input, "clearS") + 1) == 6) {
		clear_sector(atoi(afterSpace(input)));
	} else if (strcmp("drives", input) == 0) {
		if (mp == 0 || mp48 == 0) {
			kprint("Primary IDE, Master Drive (Drive 1): Online\n");
		} else {
			kprint("Primary IDE, Master Drive (Drive 1): Offline\n");
		}
		if (ms == 0 || ms48 == 0) {
			kprint("Primary IDE, Slave Drive (Drive 2): Online\n");
		} else {
			kprint("Primary IDE, Slave Drive (Drive 2): Offline\n");
		}
		if (sp == 0 || sp48 == 0) {
			kprint("Secondary IDE, Master Drive (Drive 3): Online\n");
		} else {
			kprint("Secondary IDE, Master Drive (Drive 3): Offline\n");
		}
		if (ss == 0 || ss48 == 0) {
			kprint("Secondary IDE, Slave Drive (Drive 4): Online\n");
		} else {
			kprint("Secondary IDE, Slave Drive (Drive 4): Offline\n");
		}
	} else if (strcmp("fatTest", input) == 0) {
		// dir_entry_t *new_file_created = kmalloc(sizeof(dir_entry_t));
		// uint32_t *data_to_write = kmalloc(512);
		// uint32_t *data_to_read = kmalloc(512);
		// *data_to_write = 123456789;
		// new_file("test", "txt", (uint32_t)(&new_file_created), 512, 1);
		// write_data_to_entry(new_file_created, data_to_write, 512);
		// read_data_from_entry(new_file_created, data_to_read);
		// sprint("\nData read from ");
		// kprint("Data read from ");
		// char filename[13];
		// fat_str(new_file_created->name, new_file_created->ext, filename);
		// sprint(filename);
		// sprint(": ");
		// sprint_uint(*data_to_read);
		// sprint("\n");
		// kprint(filename);
		// kprint(": ");
		// kprint_uint(*data_to_read);
		// kprint("\n");
		// free(data_to_read, 512);
		// free(data_to_write, 512);
		// free(new_file_created, sizeof(dir_entry_t));
		kprint("Big ded\n");
	} else {
		kprint("Unknown command: ");
		kprint(input);
		p_tone(100, 5);
	}
	kprint("\n");
	kprint("drip@DripOS> ");
}