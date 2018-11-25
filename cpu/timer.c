#include "timer.h"
#include "isr.h"
#include "ports.h"
#include "../libc/function.h"
#include "../drivers/screen.h"
#include "sysState.h"
#include "../drivers/keyboard.h"
#include "../drivers/stdin.h"
#include "soundManager.h"
#include "../libc/string.h"

u32 tick = 0;
u32 prev = 0;

static void timer_callback(registers_t regs) {
	sys_state_manager();
    tick++;
    for (int i = 0; i < 256; i++) {
        if (keytimeout[i] > 0) {
            keytimeout[i] -= 1;
            //char time[6];
            //int_to_ascii(keytimeout[i], time);
            //kprint(time);
        }
    }
    sound_on();
    sound_off();

    char done[24];
    int_to_ascii((int)tick, done);
    kprint(done);
    if (sound_en == true) {
        char done[24];
        int_to_ascii((int)tick, done);
        kprint(done);
    }
    //key_handler();
    UNUSED(regs);
}

void init_timer(u32 freq) {
    /* Install the function we just wrote */
    register_interrupt_handler(IRQ0, timer_callback);

    /* Get the PIT value: hardware clock at 1193180 Hz */
    u32 divisor = 1193180 / freq;
    u8 low  = (u8)(divisor & 0xFF);
    u8 high = (u8)( (divisor >> 8) & 0xFF);
    /* Send the command */
    port_byte_out(0x43, 0x36); /* Command port */
    port_byte_out(0x40, low);
    port_byte_out(0x40, high);

}

void wait(u32 ticks) {
	prev = tick;
	while(tick - prev < ticks) {
		
	}
}