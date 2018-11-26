#ifndef SOUND_H
#define SOUND_H

#include  <stdint.h>
//#include "../cpu/types.h"

void play_sound(uint32_t nFrequence, uint32_t ticks);
void play(uint32_t nFrequence);
void nosound();

#endif
