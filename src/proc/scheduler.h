#ifndef SCHEDULER_H
#define SCHEDULER_H
#include <stdint.h>
#include "sys/int/isr.h"
#include "klibc/dynarray.h"
#include "klibc/rangemap.h"
#include "klibc/lock.h"
#include "fs/fd.h"

#define READY 0
#define RUNNING 1
#define BLOCKED 2
#define SLEEP 3
#define RUNNING_IDLE 4
#define READY_IDLE 5

#define TASK_STACK_SIZE 0x4000
#define TASK_STACK_PAGES (TASK_STACK_SIZE + 0x1000 - 1) / 0x1000
#define VM_OFFSET 0xFFFF800000000000
#define USER_STACK 0x7FFFFFFFFFF0 // Alignment
#define USER_STACK_START (USER_STACK - TASK_STACK_SIZE + 16)

typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8, rsi, rdi, rbp, rdx, rcx, rbx, rax;
} __attribute__((packed)) syscall_reg_t;

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

    uint8_t running;
} task_t;

typedef struct kernel_worker_stack {
    struct kernel_worker_stack *next;
    struct kernel_worker_stack *prev;
    task_t thread;    
} worker_stack_t;


typedef struct {
    char name[50]; // The name of the process

    uint64_t cr3; // The cr3 this process has

    dynarray_t threads; // The threads this process has
    int64_t pid; // Process ID

    fd_entry_t **fd_table;
    int fd_table_size;

    uint64_t current_brk;
    lock_t brk_lock;

    int64_t uid; // User id of the user runnning this process
    int64_t gid; // Group id of the user running this process
    int64_t ppid; // Parent process id
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

typedef struct {
    int64_t a_type;
    union {
        long a_val;
        void *a_ptr;
        void (*a_func)();
    } a_un;
} auxv_t;

/* Scheduling */
void schedule(int_reg_t *r);
void schedule_ap(int_reg_t *r);
void schedule_bsp(int_reg_t *r);
void scheduler_init_bsp();
void scheduler_init_ap();
void yield();
void force_unlocked_schedule();

/* "API" */
thread_info_block_t *get_thread_locals();
int64_t add_new_child_thread(task_t *task, int64_t pid);
task_t *create_thread(char *name, void (*main)(), uint64_t rsp, uint8_t ring);
int64_t start_thread(task_t *thread);
int64_t new_thread(char *name, void (*main)(), uint64_t rsp, int64_t pid, uint8_t ring);
int64_t new_process(char *name, void *new_cr3);
void new_kernel_process(char *name, void (*main)());
void new_user_process(char *name, void (*virt_main)(), void (*phys_main)(), uint64_t code_size);

/* Killing stuff */
int kill_process(int64_t pid);
int kill_task(int64_t tid);

void start_test_user_task(); // bruh

/* Cringe functions for DripDBG */
void *get_thread_elem(uint64_t elem);
void ref_thread_elem(uint64_t elem);
void unref_thread_elem(uint64_t elem);
void lock_scheduler();
void unlock_scheduler();
uint64_t get_thread_list_size();

/* Process interface things */
process_t *reference_process(int64_t pid);
void deref_process(int64_t pid);

/* Process virtual memory management */
void *allocate_process_mem(int pid, uint64_t size);
int free_process_mem(int pid, uint64_t address);
int mark_process_mem_used(int pid, uint64_t addr, uint64_t size);
int map_user_memory(int pid, void *phys, void *virt, uint64_t size, uint16_t perms);

void *psuedo_mmap(void *base, uint64_t len);
int munmap(char *addr, uint64_t len);

/* Fork, exec, etc */
int fork();

extern uint8_t scheduler_started;
extern uint8_t scheduler_enabled;

#endif