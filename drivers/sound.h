#ifndef SOUND_H
#define SOUND_H

#include <stdint.h>

void play_sound(uint32_t nFrequence, uint32_t ticks); // Setup a sound for a length of time
void play(uint32_t nFrequence); // Play a frequency
void no_sound(); // Stop sounds
void sound_handler(); // Kernel task to handle sounds

#endif
