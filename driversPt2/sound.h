#ifndef SOUND_H
#define SOUND_H

#include  <stdint.h>
//#include "../cpuPt2/types.h"

void play_sound(uint32_t nFrequence, uint32_t ticks);
void play(uint32_t nFrequence);
void nosound();

#endif
