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

u32 tick = 0;
u32 prev = 0;
int lSnd = 0;
//int sndT = 0;
int pSnd = 0;

static void timer_callback(registers_t regs) {
	sys_state_manager();
    tick++;
    for (int i = 0; i < 256; i++) {
        if (keytimeout[i] > 0) {
            keytimeout[i] -= 1;
        }
    }
    if (tick - pSnd > lSnd) {
        nosound();
        //kprint("Timed out");
    }
    //char done[24];
    //int_to_ascii((int)tick, done);
    //kprint(done);
    //kprint("\n");
    //if (sound_en == true) {
    //    char done[24];
    //    int_to_ascii((int)tick, done);
    //    kprint(done);
    //}
    //key_handler();
    UNUSED(regs);
}

void config_timer(u32 time) {
    /* Get the PIT value: hardware clock at 1193180 Hz */
    u32 divisor = 1193180 / time;
    u8 low  = (u8)(divisor & 0xFF);
    u8 high = (u8)( (divisor >> 8) & 0xFF);
    /* Send the command */
    port_byte_out(0x43, 0x36); /* Command port */
    port_byte_out(0x40, low);
    port_byte_out(0x40, high);
    //kprint("Timer configured\n");
}

void init_timer(u32 freq) {
    /* Install the function we just wrote */
    register_interrupt_handler(IRQ0, timer_callback);
    config_timer(freq);
}

void wait(u32 ticks) {
	prev = tick;
    //kprint("Waiting...\n");
	while(tick - prev < ticks) {
		
	}
    //kprint("Finished waiting\n");
}
