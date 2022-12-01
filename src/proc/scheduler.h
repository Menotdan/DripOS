#ifndef SCHEDULER_H
#define SCHEDULER_H
#include <stdint.h>
#include "sleep_queue.h"
#include "sys/int/isr.h"
#include "klibc/rangemap.h"
#include "klibc/hashmap.h"
#include "klibc/lock.h"
#include "fs/fd.h"
#include "proc/process_management.h"

#define READY 0
#define RUNNING 1
#define BLOCKED 2
#define SLEEP 3
#define WAIT_EVENT 4
#define WAIT_EVENT_TIMEOUT 5

#define TASK_STACK_SIZE 0x4000
#define TASK_STACK_PAGES (TASK_STACK_SIZE + 0x1000 - 1) / 0x1000
#define VM_OFFSET 0xFFFF800000000000

#define USER_STACK_SIZE 0x200000
#define USER_STACK_PAGES (USER_STACK_SIZE + 0x1000 - 1) / 0x1000
#define USER_STACK 0x7FFFFFFFFFF0 // Alignment
#define USER_STACK_START (USER_STACK - USER_STACK_SIZE + 16)

/* Scheduling */
void schedule(int_reg_t *r);
void schedule_ap(int_reg_t *r);
void schedule_bsp(int_reg_t *r);
void scheduler_init_bsp();
void scheduler_init_ap();
void yield();
void force_unlocked_schedule();

/* "API" */
int64_t add_new_child_thread(thread_t *task, int64_t pid);
int64_t add_new_child_thread_no_stack_init(thread_t *thread, int64_t pid);
thread_t *create_thread(char *name, void (*main)(), uint64_t rsp, uint8_t ring);
int64_t start_thread(thread_t *thread);
int64_t new_thread(char *name, void (*main)(), uint64_t rsp, int64_t pid, uint8_t ring);
int64_t new_process(char *name, void *new_cr3);
void new_kernel_process(char *name, void (*main)());
void new_user_process(char *name, void (*virt_main)(), void (*phys_main)(), uint64_t code_size);
int64_t add_process(process_t *process);
process_t *create_process(char *name, void *new_cr3);

/* Argument passing */
void add_argv(main_thread_vars_t *vars, char *string);
void add_auxv(main_thread_vars_t *vars, auxv_t *auxv);
void add_env(main_thread_vars_t *vars, char *string);

/* Killing stuff */
int kill_process(int64_t pid);
int kill_task(int64_t tid);

void start_test_user_task(); // bruh

/* Process virtual memory management */
void *allocate_process_mem(int pid, uint64_t size);
int free_process_mem(int pid, uint64_t address);
int mark_process_mem_used(int pid, uint64_t addr, uint64_t size);
int map_user_memory(int pid, void *phys, void *virt, uint64_t size, uint16_t perms);

void *psuedo_mmap(void *base, uint64_t len, syscall_reg_t *r);
int munmap(char *addr, uint64_t len);

/* Fork, exec, etc */
int fork(syscall_reg_t *r);
void execve(char *executable_path, char **argv, char **envp, syscall_reg_t *r);
int futex_wake(uint32_t *futex);
int futex_wait(uint32_t *futex, uint32_t expected_value);
void set_fs_base_syscall(uint64_t base);

void start_idle();
void end_idle();

extern uint8_t scheduler_started;
extern uint8_t scheduler_enabled;
extern interrupt_safe_lock_t sched_lock;
extern uint64_t process_count;

extern uint64_t threads_list_size;
extern uint64_t process_list_size;
extern thread_t **threads;
extern process_t **processes;

#endif