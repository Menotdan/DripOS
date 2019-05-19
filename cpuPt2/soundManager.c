#include "soundManager.h"
#include "../driversPt2/sound.h"
#include <stdint.h>

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