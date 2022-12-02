#ifndef PROCESS_MANAGE_H
#define PROCESS_MANAGE_H
#include <stdint.h>
#include "fs/fd.h"
#include "klibc/hashmap.h"
#include "proc/sleep_queue.h"

#define DEFAULT_BRK 0x10000000000

typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8, rsi, rdi, rbp, rdx, rcx, rbx, rax;
} __attribute__((packed)) syscall_reg_t;

typedef struct {
    uint64_t rax, rbx, rcx, rdx, rbp, rdi, rsi, r8, r9, r10, r11, r12, r13, r14, r15;
    uint64_t rsp, rip;
    uint64_t ss, cs, fs;
    uint64_t rflags;
    uint64_t cr3;
    uint32_t mxcsr;
    uint16_t fcw;
} task_regs_t;

typedef struct {
    int64_t a_type;
    union {
        long a_val;
        void *a_ptr;
        void (*a_func)();
    } a_un;
} auxv_t;

typedef struct {
    auxv_t *auxv;
    int auxc;
} auxv_auxc_group_t;

typedef struct {
    char **argv;
    char **enviroment;
    auxv_t *auxv;

    int argc;
    int envc;
    int auxc;
} main_thread_vars_t;

typedef struct {
    char name[50]; // The name of the process

    uint64_t cr3; // The cr3 this process has

    int64_t *threads;
    uint64_t threads_size;

    int64_t pid; // Process ID

    fd_entry_t **fd_table;
    int fd_table_size;

    uint64_t current_brk;
    lock_t brk_lock;

    uint64_t local_watchpoint1; // DR2
    uint64_t local_watchpoint2; // DR3
    uint8_t local_watchpoint1_active;
    uint8_t local_watchpoint2_active;

    lock_t ipc_create_handle_lock;
    hashmap_t *ipc_handles; // a hashmap of port no -> ipc_handle_t *

    int64_t uid; // User id of the user runnning this process
    int64_t gid; // Group id of the user running this process
    int64_t ppid; // Parent process id
    uint64_t permissions; // Misc permission flags
} process_t;

typedef struct {
    char name[50]; // The name of the task

    task_regs_t regs; // The task's registers

    uint64_t kernel_stack;
    uint64_t user_stack;

    uint64_t tsc_started; // The last time this task was started
    uint64_t tsc_stopped; // The last time this task was stopped
    uint64_t tsc_total; // The total time this task has been running for

    uint8_t state; // State of the task
    int cpu; // CPU the task is running on

    int64_t tid; // Task ID
    int64_t parent_pid; // The pid of the parent process
    process_t *parent; // A pointer to the parent for some code to use
    uint8_t ring;

    uint8_t running;
    sleep_queue_t *sleep_node;

    main_thread_vars_t vars;

    int *event;
    uint64_t event_timeout;
    uint64_t event_wait_start;

    uint8_t ignore_ring; // for error checking

    char sse_region[512] __attribute__((aligned(16))); // SSE region (aligned)
} thread_t;

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

int64_t add_new_pid(int locked);
int64_t add_new_tid(int locked);

#endif