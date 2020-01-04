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
#include "../drivers/stdin.h"
#include "../drivers/vesa.h"
#include "../builtinapps/snake.h"
#include <cpuid.h>

int arg = 0; //Is an argument being taken?
int argt = 0; //Which Command Is taking the argument?
uint32_t task2 = 0;
Task bg_task_timer;
//char *current_buffer;
//char *previous_buffer;
//uint32_t current_buffer_pos = 0;
//uint32_t previous_buffer_pos = 0;
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
		while (tempTick >= 1000)
		{
			tempTick -= 1000;
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
	} else if (strcmp(input, "snake") == 0) {
		createTask(kmalloc(sizeof(Task)), snake_main, "Snake game");
		kprint("Snake started!");
	} else if (strcmp(input, "help") == 0) {
		kprint("Commands: ps, kill, uptime, scan, testDrive, fmem, help, shutdown, panic, print, clear, bgtask, bgoff, read, drives, select, testMem, free\n");
	} else if (strcmp(input, "cpu") == 0) {
		char cpuid_output[4][4]; // Four uint8_ts
		__get_cpuid(0, (unsigned int *)cpuid_output[0], (unsigned int *)cpuid_output[1], (unsigned int *)cpuid_output[2], (unsigned int *)cpuid_output[3]);
		kprint((char *)cpuid_output);
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
		uint32_t tone_freq = atoi(afterSpace(input));
		play_sound(tone_freq, 500);
	} else if (strcmp(input, "bgtask") == 0) {
		Task *temp_task1 = kmalloc(sizeof(Task));
		Task *temp_task2 = kmalloc(sizeof(Task));

		task = createTask(temp_task1, bg_task, "Tick counter");
		task2 = createTask(temp_task2, bg_task2, "Tick counter");
		temp_task1->priority = LOW;
		temp_task2->priority = LOW;

		kprint("Background task started!");
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
		for (int c = 0; c < 10000; c++) {
			testy = (char *)kmalloc(0x1000);
			kprint_uint((uint32_t)testy);
			kprint("\n");
			*testy = 100;
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
		read_rtc();
		kprint_int(year);
		kprint("/");
		kprint_int(month);
		kprint("/");
		kprint_int(day);
		if(hour - 5 <= 10) {
			kprint(" 0");
			kprint_int(hour);
		} else {
			kprint(" ");
			kprint_int(hour);
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
	} else if (strcmp(input, "testInputSystem") == 0) {
		kprint(getline_print(0));
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
		vesa_buffer_t test = new_framebuffer((width/2)+1, 0, (width/2), height);
		vesa_buffer_t temp = swap_display(test);
		uint32_t cursor_off = get_cursor_offset();
		set_cursor_offset(0);
		kprint("Second framebuffer test\n");
		test = swap_display(temp);
		set_cursor_offset(cursor_off);
		cleanup_framebuffer(test);
		sleep(1234);
	} else {
		kprint("Unknown command: ");
		kprint(input);
		play_sound(100, 500);
	}
	kprint("\n");
	kprint("drip@DripOS> ");
}

uint8_t key_handler(uint8_t scancode, bool keyup, uint8_t shift) {
    if (scancode == LARROW && keyup != true){
        if (position > 0) {
            position -= 1;
            int cOffset = get_cursor_offset();
            set_cursor_offset(get_offset(get_offset_col(cOffset)-1, get_offset_row(cOffset)));
        }
    } else if (scancode == RARROW && keyup != true){
        if (position < uinlen) {
            position += 1;
            int cOffset = get_cursor_offset();
            set_cursor_offset(get_offset(get_offset_col(cOffset)+1, get_offset_row(cOffset)));
        }
    } else if (scancode == UPARROW && keyup != true){
        uint32_t loop = 0;
        uint32_t loop2 = 0;
        while (loop2 < (uint32_t)strlen(key_buffer))
        {
            key_buffer_down[loop] = key_buffer[loop];
            loop2++;
        }
        
        while (loop < (uint32_t)strlen(key_buffer_up))
        {
            key_buffer[loop] = key_buffer_up[loop];
            loop++;
        }
        key_buffer[strlen(key_buffer_up)] = remove_null("\0");

        int cOffset = get_cursor_offset();
        set_cursor_offset(get_offset(get_offset_col(cOffset)+uinlen-position, get_offset_row(cOffset)));
        for (uint32_t g = 0; g < uinlen; g++)
        {
            kprint_backspace();
        }
        //kprint_backspace();
        cOffset = get_cursor_offset();
        set_cursor_offset(get_offset(get_offset_col(cOffset), get_offset_row(cOffset)));
        if (key_buffer != 0) {
            kprint(key_buffer);
        }
        //sprintd(key_buffer);
        //set_cursor_offset(get_offset(get_offset_col(offsetTemp)-1, get_offset_row(offsetTemp)));
        uinlen = strlen(key_buffer_up);
        position = uinlen;
    }
    else if (strcmp(sc_name[scancode], "Backspace") == 0 && keyup != true) {
        if (uinlen > 0) {
            backspacep(key_buffer, position);
            uint32_t offsetTemp = get_cursor_offset();
            int cOffset = get_cursor_offset();
            set_cursor_offset(get_offset(get_offset_col(cOffset)+uinlen-position, get_offset_row(cOffset)));
            for (uint32_t g = 0; g < uinlen; g++)
            {
                kprint_backspace();
            }
            //kprint_backspace();
            cOffset = get_cursor_offset();
            set_cursor_offset(get_offset(get_offset_col(cOffset), get_offset_row(cOffset)));
            if (key_buffer != 0) {
                kprint(key_buffer);
            }
            //sprintd(key_buffer);
            set_cursor_offset(get_offset(get_offset_col(offsetTemp)-1, get_offset_row(offsetTemp)));
            uinlen -= 1;
            position -= 1;
        }
    } else if (strcmp(sc_name[scancode], "Spacebar") == 0 && keyup != true) {
        appendp(key_buffer, sc_ascii[scancode], position);
        uint32_t offsetTemp = get_cursor_offset();
        int cOffset = get_cursor_offset();
        set_cursor_offset(get_offset(get_offset_col(cOffset)+uinlen-position, get_offset_row(cOffset)));
        for (uint32_t g = 0; g < uinlen; g++)
        {
            kprint_backspace();
        }
        cOffset = get_cursor_offset();
        if (key_buffer != 0) {
            kprint(key_buffer);
        }
        set_cursor_offset(get_offset(get_offset_col(offsetTemp)+1, get_offset_row(offsetTemp)));
        uinlen++;
        position++;
    } else if (strcmp(sc_name[scancode], "ERROR") == 0 && keyup != true) {
    } else if (strcmp(sc_name[scancode], "Lctrl") == 0 && keyup != true) {
    } else if (strcmp(sc_name[scancode], "LAlt") == 0 && keyup != true) {
    } else if (strcmp(sc_name[scancode], "LShift") == 0 && keyup != true) {
        shift = 1;
    } else if (strcmp(sc_name[scancode], "RShift") == 0 && keyup != true) {
        shift = 1;
    }  else if (strcmp(sc_name[scancode], "LShift") == 0 && keyup == true) {
        shift = 0;
    } else if (strcmp(sc_name[scancode], "RShift") == 0 && keyup == true) {
        shift = 0;
    } else if (strcmp(sc_name[scancode], "Enter") == 0 && keyup != true) {
        for (uint32_t q = 0; q < uinlen; q++) {
            key_buffer_up[q] = key_buffer[q];
            key_buffer_up[q+1] = '\0';
        }
        user_input(key_buffer);
        for (uint32_t i = 0; i < 2000; i++) {
			key_buffer[i] = 0;
		}
        uinlen = 0;
		position = 0;
    } else {
        if (shift == 0) {
            if (!keyup) {
                if (scancode < SC_MAX){
                    appendp(key_buffer, sc_ascii[scancode], position);

                    uint32_t offsetTemp = get_cursor_offset();
                    int cOffset = get_cursor_offset();
                    set_cursor_offset(get_offset(get_offset_col(cOffset)+(uinlen-position), get_offset_row(cOffset)));
                    cOffset = get_cursor_offset();
                    for (uint32_t g = 0; g < uinlen; g++)
                    {
                        kprint_backspace();
					}
                    if (key_buffer != 0) {
                        kprint(key_buffer);
                    }
                    set_cursor_offset(get_offset(get_offset_col(offsetTemp)+1, get_offset_row(offsetTemp)));
                    uinlen++;
                    position++;
                }
            }
        } else if (shift == 1) {
            if (!keyup) {
                if (scancode < SC_MAX){
                    appendp(key_buffer, sc_ascii_uppercase[scancode], position);

                    uint32_t offsetTemp = get_cursor_offset();
                    int cOffset = get_cursor_offset();
                    set_cursor_offset(get_offset(get_offset_col(cOffset)+uinlen-position, get_offset_row(cOffset)));
                    for (uint32_t g = 0; g < uinlen; g++)
                    {
                        kprint_backspace();
                    }
                    cOffset = get_cursor_offset();
                    if (key_buffer != 0) {
                        kprint(key_buffer);
					}

                    set_cursor_offset(get_offset(get_offset_col(offsetTemp)+1, get_offset_row(offsetTemp)));
                    uinlen++;
                    position++;
                }
            }
        }
    }
	return shift;
}

void terminal_task() {
	uint8_t shift = 0;
	uint32_t prev_offset = 0;
	color_t under_cursor[6];
	color_t under_cursor_new[6];
	under_cursor[0] = color_from_rgb(0,0,0);
	under_cursor[1] = color_from_rgb(0,0,0);
	under_cursor[2] = color_from_rgb(0,0,0);
	under_cursor[3] = color_from_rgb(0,0,0);
	under_cursor[4] = color_from_rgb(0,0,0);
	under_cursor[5] = color_from_rgb(0,0,0);
	while (1)
	{
		unsigned char scan = (unsigned char)getcode(); // Waiting for a scancode from the keyboard
		if (scan > 0x80) {
			scan = scan - 0x80;
			shift = key_handler(scan, true, shift);
		} else {
			shift = key_handler(scan, false, shift);
			uint32_t cur_x = (uint32_t)get_offset_col(get_cursor_offset());
			uint32_t cur_y = (uint32_t)get_offset_row(get_cursor_offset());
			cur_x = (cur_x*8)+1;
			cur_y = cur_y*8;
			cur_y += 10;
			under_cursor_new[0] = get_pixel(cur_x,cur_y);
			draw_pixel(cur_x, cur_y, 255, 255, 255);
			cur_x++;
			under_cursor_new[1] = get_pixel(cur_x,cur_y);
			draw_pixel(cur_x, cur_y, 255, 255, 255);
			cur_x++;
			under_cursor_new[2] = get_pixel(cur_x,cur_y);
			draw_pixel(cur_x, cur_y, 255, 255, 255);
			cur_x++;
			under_cursor_new[3] = get_pixel(cur_x,cur_y);
			draw_pixel(cur_x, cur_y, 255, 255, 255);
			cur_x++;
			under_cursor_new[4] = get_pixel(cur_x,cur_y);
			draw_pixel(cur_x, cur_y, 255, 255, 255);
			cur_x++;
			under_cursor_new[5] = get_pixel(cur_x,cur_y);
			draw_pixel(cur_x, cur_y, 255, 255, 255);

			cur_x = (uint32_t)get_offset_col(prev_offset);
			cur_y = (uint32_t)get_offset_row(prev_offset);
			cur_x = (cur_x*8)+1;
			cur_y = cur_y*8;
			cur_y += 10;
			draw_pixel(cur_x, cur_y, under_cursor[0].red, under_cursor[0].green, under_cursor[0].blue);
			cur_x++;
			draw_pixel(cur_x, cur_y, under_cursor[1].red, under_cursor[1].green, under_cursor[1].blue);
			cur_x++;
			draw_pixel(cur_x, cur_y, under_cursor[2].red, under_cursor[2].green, under_cursor[2].blue);
			cur_x++;
			draw_pixel(cur_x, cur_y, under_cursor[3].red, under_cursor[3].green, under_cursor[3].blue);
			cur_x++;
			draw_pixel(cur_x, cur_y, under_cursor[4].red, under_cursor[4].green, under_cursor[4].blue);
			cur_x++;
			draw_pixel(cur_x, cur_y, under_cursor[5].red, under_cursor[5].green, under_cursor[5].blue);

			update_display();

			under_cursor[0] = under_cursor_new[0];
			under_cursor[1] = under_cursor_new[1];
			under_cursor[2] = under_cursor_new[2];
			under_cursor[3] = under_cursor_new[3];
			under_cursor[4] = under_cursor_new[4];
			under_cursor[5] = under_cursor_new[5];
			prev_offset = (uint32_t)get_cursor_offset();
		}
	}
}

void init_terminal() {
	//sprint_uint(123456);
	//current_buffer = kmalloc(2000);
	//previous_buffer = kmalloc(2000);
	Task *term = kmalloc(sizeof(Task));
	uint8_t terminal_pid = createTask(term, terminal_task, "Terminal");
	set_focused_task(term);
	if (terminal_pid){};
}