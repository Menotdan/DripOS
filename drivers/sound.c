#include <stdlib.h>
#include "sound.h"
#include "../cpu/ports.h"
#include "../cpu/timer.h"
#include "../cpu/task.h"

uint32_t start_sound; // The start of a sound
uint32_t len_sound; // Length of the sound

void play(uint32_t nFrequence) {
    uint32_t Div;
    uint8_t tmp;
    if (nFrequence > 0) {
        Div = 1193180 / nFrequence;
        port_byte_out(0x43, 0xb6);
        port_byte_out(0x42, (uint8_t) (Div) );
        port_byte_out(0x42, (uint8_t) (Div >> 8));

        tmp = port_byte_in(0x61);
        if (tmp != (tmp | 3)) {
            port_byte_out(0x61, tmp | 3);
        }
    } else {
        Div = 1193180 / 1;
        port_byte_out(0x43, 0xb6);
        port_byte_out(0x42, (uint8_t) (Div) );
        port_byte_out(0x42, (uint8_t) (Div >> 8));

        tmp = port_byte_in(0x61);
        if (tmp != (tmp | 3)) {
            port_byte_out(0x61, tmp | 3);
        }
    }
    
}

void nosound() {
    uint8_t tmp = port_byte_in(0x61) & 0xFC;

    port_byte_out(0x61, tmp);
}

void play_sound(uint32_t nFrequence, uint32_t ticks) {
    play(nFrequence); // Set the PIT for the speaker to play the frequency requested
    start_sound = tick; // Current timer tick
    len_sound = ticks; // Length
    Task *sound_handle_task = kmalloc(sizeof(Task));
    createTask(sound_handle_task, sound_handler, "Sound stopper");
    sprintf("\nSound handler PID: %u", sound_handle_task->pid);
}

void pgw() {
    play_sound(300, 30);
    play_sound(400, 30);
    wait(10);
    play_sound(500, 30);
    play_sound(600, 30);
}

void sound_handler() {
    while (1) {
        /* If the sound is done playing, stop it, otherwise switch tasks */
        sleep(len_sound);
        if ((tick - start_sound >= len_sound)) {
            nosound();
            exit();
        } else
        {
            sleep(10);
        }
        
    }
}