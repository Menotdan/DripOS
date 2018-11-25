#ifndef SOUND_H
#define SOUND_H

#include "../cpu/types.h"

void play_sound(u32 nFrequence, u32 ticks);
void play(u32 nFrequence);
void nosound();

#endif