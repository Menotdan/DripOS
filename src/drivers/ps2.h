#ifndef PS2_H
#define PS2_H
#include <stdint.h>
#include "sys/int/isr.h"

#define MOUSE_SENSITIVITY 3

void keyboard_handler(int_reg_t *r);
void mouse_handler(int_reg_t *r);
void mouse_setup();

#endif