#ifndef KLIBC_DEBUG_H
#define KLIBC_DEBUG_H
#include <stdint.h>

void init_debugger();
void set_debug_state();
void set_watchpoint(void *address, uint8_t watchpoint_index);
void debug_handler();

#endif