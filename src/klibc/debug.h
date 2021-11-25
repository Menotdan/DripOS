#ifndef KLIBC_DEBUG_H
#define KLIBC_DEBUG_H
#include <stdint.h>
#include <proc/scheduler.h>

void init_debugger();
void set_debug_state();
uint64_t create_local_dr7(thread_t *t);
void set_watchpoint(void *address, uint8_t watchpoint_index);
void clear_global_watchpoint(uint8_t watchpoint_index);
void stack_trace(int_reg_t *r, uint64_t max_frames);
void set_local_watchpoint(void *address, uint8_t watchpoint_index);
void debug_handler(int_reg_t *r);

#endif