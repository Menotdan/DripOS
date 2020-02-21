#ifndef SCHEDULER_H
#define SCHEDULER_H
#include <stdint.h>
#include "sys/int/isr.h"

#define READY 0
#define RUNNING 1
#define BLOCKED 2
#define IRQ_WAIT 3
#define SLEEP 4

#define TASK_STACK 0x4000

typedef struct {
    uint64_t rax, rbx, rcx, rdx, rbp, rdi, rsi, r8, r9, r10, r11, r12, r13, r14, r15;
    uint64_t rsp, rip;
    uint64_t ss, cs, fs;
    uint64_t rflags;
    uint64_t cr3;
} task_regs_t;

typedef struct {
    task_regs_t regs;
    uint64_t times_selected; // Times the task has been selected since last new task
    uint8_t state; // State of the task
    uint8_t cpu; // CPU the task is running on
    uint8_t waiting_irq; // The IRQ this task is waiting for

    uint64_t pid;
} task_t;

typedef struct {
    uint64_t meta_pointer;
} __attribute__((packed)) thread_info_block_t;


void schedule(int_reg_t *r);
void scheduler_init_bsp();
void enter_task();
void exit_task();
uint8_t cpu_in_task();

extern uint8_t scheduler_start;

#endif