#ifndef SCHEDULER_H
#define SCHEDULER_H
#include <stdint.h>
#include "sys/int/isr.h"
#include "klibc/dynarray.h"

#define READY 0
#define RUNNING 1
#define BLOCKED 2
#define SLEEP 3
#define RUNNING_IDLE 4
#define READY_IDLE 5

#define TASK_STACK_SIZE 0x4000
#define TASK_STACK_PAGES (TASK_STACK_SIZE + 0x1000 - 1) / 0x1000
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

    uint64_t kernel_stack;
    uint64_t user_stack;

    uint64_t tsc_started; // The last time this task was started
    uint64_t tsc_stopped; // The last time this task was stopped
    uint64_t tsc_total; // The total time this task has been running for

    uint8_t state; // State of the task
    uint8_t cpu; // CPU the task is running on

    int64_t tid; // Task ID
    int64_t parent_pid; // The pid of the parent process
    uint8_t ring;
} task_t;

typedef struct {
    char name[50]; // The name of the process

    uint64_t cr3; // The cr3 this process has

    dynarray_t threads; // The threads this process has
    int64_t pid; // Process ID

    int64_t uid; // User id of the user runnning this process
    int64_t gid; // Group id of the user running this process
    uint64_t permissions; // Misc permission flags
} process_t;

typedef struct {
    /* Do NOT remove these */
    uint64_t meta_pointer;
    /* These are fine to move around and change types of */

    int errno;

    int64_t tid;
    uint64_t tsc_started; // The last time this task was started
    uint64_t tsc_stopped; // The last time this task was stopped
    uint64_t tsc_total; // The total time this task has been running for
} __attribute__((packed)) thread_info_block_t;


void schedule(int_reg_t *r);
void schedule_ap(int_reg_t *r);
void schedule_bsp(int_reg_t *r);
void scheduler_init_bsp();
void scheduler_init_ap();

thread_info_block_t *get_thread_locals();
int64_t add_new_thread(task_t *task);
int64_t add_new_child_thread(task_t *task, int64_t pid);
task_t *create_thread(char *name, void (*main)(), uint64_t rsp, uint8_t ring);
int64_t new_thread(char *name, void (*main)(), uint64_t rsp, int64_t pid, uint8_t ring);
int64_t new_process(char *name, void *new_cr3);
void new_kernel_process(char *name, void (*main)());
void new_user_process(char *name, void (*virt_main)(), void (*phys_main)(), uint64_t code_size);

int kill_process(int64_t pid);
int kill_task(int64_t tid);

void start_test_user_task();

extern uint8_t scheduler_started;
extern uint8_t scheduler_enabled;

#endif