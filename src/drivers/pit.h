#ifndef PIT_H
#define PIT_H
#include <stdint.h>
#include "sys/int/isr.h"

void timer_handler(int_reg_t *r);
void set_pit_freq();
void sleep_no_task(uint64_t ticks);

extern uint64_t global_ticks;

#endif