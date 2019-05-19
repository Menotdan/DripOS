#ifndef TIMER_H
#define TIMER_H

#include "types.h"

void init_timer(uint32_t freq);
void wait(uint32_t ticks);
uint32_t tick;
int lSnd;
int pSnd;
int task;

#endif
