#pragma once

#include <stdint.h>
#include <mem.h>
#include <drivers/vesa.h>
#include <drivers/ps2.h>
#include <drivers/sound.h>
#include <drivers/stdin.h>

typedef struct segment {
    struct segment *next;
    uint8_t next_dir;
    uint32_t x;
    uint32_t y;
} segment_t;


struct snake {
    uint32_t score;
    segment_t *head;
};

struct apple {
    uint32_t x;
    uint32_t y;
};

void snake_main();