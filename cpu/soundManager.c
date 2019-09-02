#include "soundManager.h"
#include "../drivers/sound.h"
#include "../libc/stdint.h"

bool sound_en = false;
bool sound_dis = true;
int sound = 0;

void sound_off() {
    if (sound_dis == true) {
        nosound();
    }
}

void sound_on() {
    if (sound_en == true) {
        if (sound > 0) {
            play((uint32_t) sound);
        }
    }
}