#ifndef TIMER_H
#define TIMER_H

#include "types.h"
#include "isr.h"

void init_timer(uint32_t freq);
void wait(uint32_t ms);
uint32_t tick;
int lSnd;
int pSnd;
int task;
uint32_t switch_task;
registers_t *temp;

#endif
