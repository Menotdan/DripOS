#ifndef SCHEDULER_H
#define SCHEDULER_H
#include <stdint.h>
#include "sys/int/isr.h"
#include "klibc/dynarray.h"

#define READY 0
#define RUNNING 1
#define BLOCKED 2
#define IRQ_WAIT 3
#define SLEEP 4

#define TASK_STACK_SIZE 0x4000
#define VM_OFFSET 0xFFFF800000000000

typedef struct {
    uint64_t rax, rbx, rcx, rdx, rbp, rdi, rsi, r8, r9, r10, r11, r12, r13, r14, r15;
    uint64_t rsp, rip;
    uint64_t ss, cs, fs;
    uint64_t rflags;
    uint64_t cr3;
} task_regs_t;

typedef struct {
    char name[50]; // The name of the task

    task_regs_t regs; // The task's registers
    
    uint64_t tsc_started; // The last time this task was started
    uint64_t tsc_stopped; // The last time this task was stopped
    uint64_t tsc_total; // The total time this task has been running for

    uint8_t state; // State of the task
    uint8_t cpu; // CPU the task is running on
    uint8_t waiting_irq; // The IRQ this task is waiting for

    int64_t tid; // Task ID
    int64_t parent_pid; // The pid of the parent process
} task_t;

typedef struct {
    char name[50]; // The name of the process

    dynarray_t threads; // The threads this process has
    int64_t pid; // Process ID
} process_t;

typedef struct {
    uint64_t meta_pointer;
    int64_t tid;
    uint64_t tsc_started; // The last time this task was started
    uint64_t tsc_stopped; // The last time this task was stopped
    uint64_t tsc_total; // The total time this task has been running for
} __attribute__((packed)) thread_info_block_t;


void schedule(int_reg_t *r);
void scheduler_init_bsp();

task_t *new_task(void (*main)(), void *parent_addr_space_cr3, char *name);
int new_process(void (*main)(), void *parent_addr_space_cr3, char *name);


extern uint8_t scheduler_started;
extern uint8_t scheduler_enabled;

#endif