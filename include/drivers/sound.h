/*
    sound.h
    Copyright Menotdan 2018-2019

    Sound driver

    MIT License
*/

#pragma once

#include  <stdint.h>
#include "../../cpu/ports.h"
#include "../../cpu/timer.h"
#include "../../cpu/task.h"

void play_sound(uint32_t nFrequence, uint32_t ticks); // Setup a sound for a length of time
void play(uint32_t nFrequence); // Play a frequency
void nosound(); // Stop sounds
void sound_handler(); // Kernel task to handle sounds
