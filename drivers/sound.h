#ifndef SOUND_H
#define SOUND_H

#include  <stdint.h>
//#include "../cpu/types.h"

void play_sound(uint32_t nFrequence, uint32_t ticks); // Setup a sound for a length of time
void play(uint32_t nFrequence); // Play a frequency
void nosound(); // Stop sounds
void sound_handler(); // Kernel task to handle sounds

#endif
