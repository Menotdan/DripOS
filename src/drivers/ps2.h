#ifndef PS2_H
#define PS2_H
#include <stdint.h>
#include "sys/int/isr.h"

void keyboard_handler(int_reg_t *r);

#endif