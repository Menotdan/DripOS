#include "sound.h"

#include "../cpu/types.h"
#include "../cpu/ports.h"
#include "../cpu/timer.h"

void play(u32 nFrequence) {
    u32 Div;
    u8 tmp;

    Div = 1193180 / nFrequence;
    port_byte_out(0x43, 0xb6);
    port_byte_out(0x42, (u8) (Div) );
    port_byte_out(0x42, (u8) (Div >> 8));

    tmp = port_byte_in(0x61);
    if (tmp != (tmp | 3)) {
        port_byte_out(0x61, tmp | 3);
    }
}

void nosound() {
    u8 tmp = port_byte_in(0x61) & 0xFC;

    port_byte_out(0x61, tmp);
}

void play_sound(u32 nFrequence, u32 ticks) {
    play(nFrequence);
    wait(ticks);
    nosound();
}