#include "sound.h"
//#include "../cpu/types.h"
#include "../cpu/ports.h"
#include "../cpu/timer.h"

void play(uint32_t nFrequence) {
    uint32_t Div;
    uint8_t tmp;

    Div = 1193180 / nFrequence;
    port_byte_out(0x43, 0xb6);
    port_byte_out(0x42, (uint8_t) (Div) );
    port_byte_out(0x42, (uint8_t) (Div >> 8));

    tmp = port_byte_in(0x61);
    if (tmp != (tmp | 3)) {
        port_byte_out(0x61, tmp | 3);
    }
}

void nosound() {
    uint8_t tmp = port_byte_in(0x61) & 0xFC;

    port_byte_out(0x61, tmp);
}

void play_sound(uint32_t nFrequence, uint32_t ticks) {
    play(nFrequence);
    wait(ticks);
    nosound();
}