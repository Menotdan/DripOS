#ifndef SCHEDULER_H
#define SCHEDULER_H
#include <stdint.h>
#include "sys/int/isr.h"

typedef struct {
    uint64_t rax, rbx, rcx, rdx, rbp, rdi, rsi, r8, r9, r10, r11, r12, r13, r14, r15;
    uint64_t rsp, rip;
    uint64_t ss, cs, fs;
    uint64_t cr3;
} task_regs_t;

typedef struct {
    task_regs_t regs;
    uint64_t times_selected;
} task_t;

void schedule(int_reg_t *r);

#endif