#include "rand.h"
uint32_t seed = 1;

void srand( unsigned int new_seed )
{
    seed = new_seed;
}

uint32_t rand(uint32_t max)
{
    srand(tick);
    seed = seed * 1103515245 + 12345;
    return (uint32_t)(seed / 65536) % (max+1);
}

uint32_t rand_no_tick(uint32_t max)
{
    seed = seed * 1103515245 + 12345;
    return (uint32_t)(seed / 65536) % (max+1);
}