#ifndef PIT_H
#define PIT_H
#include <stdint.h>
#include "sys/int/isr.h"

#define sched_period 8

void timer_handler(int_reg_t *r);
void set_pit_freq();
void sleep_no_task(uint64_t ticks);

uint64_t stopwatch_start();
uint64_t stopwatch_stop(uint64_t start);
void time_code(uint64_t *start, char *description);

extern volatile uint64_t global_ticks;

#endif