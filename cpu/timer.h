#ifndef TIMER_H
#define TIMER_H

#include "types.h"
#include "isr.h"

void init_timer(uint32_t freq);
void wait(uint32_t ms);
void sleep(uint32_t ms);

extern uint64_t tick;
int lSnd;
int pSnd;
int task;
extern uint32_t switch_task;
extern uint32_t time_slice_left;

#endif
