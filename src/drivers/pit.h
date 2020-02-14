#ifndef PIT_H
#define PIT_H
#include <stdint.h>
#include "sys/int/isr.h"

void timer_handler(int_reg_t *r);
void set_pit_freq(uint64_t freq);

#endif