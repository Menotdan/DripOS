#ifndef TIMER_H
#define TIMER_H

#include "types.h"

void init_timer(u32 freq);
void wait(u32 ticks);
u32 tick;
int lSnd;
int pSnd;
int task;

#endif
