#include "timer.h"
#include "isr.h"
#include "ports.h"
#include "../libc/function.h"
#include "../drivers/screen.h"
#include "sysState.h"
#include "../drivers/keyboard.h"
#include "../drivers/stdin.h"
#include "../drivers/sound.h"
#include "soundManager.h"
#include "../libc/string.h"
#include "../kernel/systemManager.h"
#include "../kernel/kernel.h"
#include "task.h"

uint32_t tick = 0; //Ticks
uint32_t prev = 0; //Previous ticks for wait() function
int lSnd = 0; //Length of sound
int task = 0; //is task on?
int pSnd = 0; //ticks before sound started
int tMil = 0; // How many million ticks
char tMilStr[12];
uint32_t okok = 0;
uint32_t timesliceleft = 1;
registers_t *temp;
uint32_t switch_task = 0;

static void timer_callback(registers_t *regs) {
    tick++;
    if (loaded == 1) {
        timesliceleft--;
    }
    //kprint("");
    if (tick - (uint32_t)pSnd > (uint32_t)lSnd*10) {
        nosound();
    }
    if((int)tick == 1000000) {
        tMil += 1;
    }

    runningTask->ticks_cpu_time++;
    UNUSED(regs);
    if (timesliceleft == 0 && loaded == 1) {
        if (runningTask->next->priority == NORMAL) {
            timesliceleft = 16; // 16 ms
        }
        if (runningTask->next->priority == HIGH) {
            timesliceleft = 24; // 24 ms
        }
        if (runningTask->next->priority == LOW) {
            timesliceleft = 8; // 8 ms
        }
        //timer_switch_task(temp, runningTask->next); // Switch task
        switch_task = 1;
    }
}

void config_timer(uint32_t time) {
    /* Get the PIT value: hardware clock at 1193180 Hz */
    uint32_t divisor = 1193180 / time;
    uint8_t low  = (uint8_t)(divisor & 0xFF);
    uint8_t high = (uint8_t)( (divisor >> 8) & 0xFF);
    /* Send the command */
    port_byte_out(0x43, 0x36); /* Command port */
    port_byte_out(0x40, low);
    port_byte_out(0x40, high);
    //kprint("Timer configured\n");
}

void init_timer(uint32_t freq) {
    /* Install the function we just wrote */
    register_interrupt_handler(IRQ0, timer_callback);
    config_timer(freq);
}

void wait(uint32_t ms) {
	prev = tick;
    //char *eghagh;
    //int_to_ascii(ticks, eghagh);
    //kprint(eghagh);
	while(tick < ms*10 + prev) {
		//logoDraw();
        //okok += 1;
        //int_to_ascii(okok, eghagh);
        //append(eghagh, "\n");
        //kprint(eghagh);
        manage_sys();
	}
}
