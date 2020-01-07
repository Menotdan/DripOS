#include <stdint.h>
#include "../libc/mem.h"
#include "../libc/rand.h"
#include "../drivers/vesa.h"
#include "../drivers/ps2.h"
#include "../drivers/sound.h"
#include "../drivers/stdin.h"

typedef struct segment
{
    struct segment *next;
    struct segment *previous;
    uint8_t next_dir;
    uint8_t moving_dir;
    uint32_t x;
    uint32_t y;
} segment_t;


typedef struct snake
{
    uint32_t score;
    segment_t *head;
} snake_t;

typedef struct apple
{
    uint32_t x;
    uint32_t y;
} apple_t;

void snake_main();