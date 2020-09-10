#ifndef SCHED_SYSCALLS_H
#define SCHED_SYSCALLS_H
#include "scheduler.h"

void *psuedo_mmap(void *base, uint64_t len, syscall_reg_t *r);
int munmap(char *addr, uint64_t len);
int fork(syscall_reg_t *r);
void execve(char *executable_path, char **argv, char **envp, syscall_reg_t *r);
void set_fs_base_syscall(uint64_t base);
int futex_wake(uint32_t *futex);
int futex_wait(uint32_t *futex, uint32_t expected_value);

#endif