#ifndef SYSCALLS_H
#define SYSCALLS_H
#include <stdint.h>
#include "proc/scheduler.h"

/* All the useful syscalls */
void syscall_read(syscall_reg_t *r);                   // 0     int fd, void *buf, uint64_t count
void syscall_write(syscall_reg_t *r);                  // 1     int fd, void *buf, uint64_t count
void syscall_open(syscall_reg_t *r);                   // 2     char *path, int mode
void syscall_close(syscall_reg_t *r);                  // 3     int fd
void syscall_getpid(syscall_reg_t *r);                 // 5
void syscall_seek(syscall_reg_t *r);                   // 8     int fd, uint64_t offset, int whence
void syscall_mmap(syscall_reg_t *r);                   // 9     void *base_addr, uint64_t size
void syscall_munmap(syscall_reg_t *r);                 // 11    void *addr, uint64_t size
void syscall_exit(syscall_reg_t *r);                   // 12    int exit_code
void syscall_getppid(syscall_reg_t *r);                // 14
void syscall_yield(syscall_reg_t *r);                  // 24
void syscall_nanosleep(syscall_reg_t *r);              // 35    timespec *req, timespec *rem
void syscall_sprint(syscall_reg_t *r);                 // 50    char *str
void syscall_fork(syscall_reg_t *r);                   // 57
void syscall_map_from_us_to_process(syscall_reg_t *r); // 58    int pid, void *local_memory, uint64_t size
void syscall_execve(syscall_reg_t *r);                 // 59    char *path, char **argv, char **envp
void syscall_ipc_read(syscall_reg_t *r);               // 60    int pid, int port, void *buf, uint64_t size
void syscall_ipc_write(syscall_reg_t *r);              // 61    int pid, int port, void *buf, uint64_t size
void syscall_ipc_register(syscall_reg_t *r);           // 62    int port
void syscall_ipc_wait(syscall_reg_t *r);               // 63    int port, syscall_ipc_handle_t *out
void syscall_ipc_handling_complete(syscall_reg_t *r);  // 64    syscall_ipc_handle_t *in
void syscall_futex_wake(syscall_reg_t *r);             // 65    uint32_t *futex
void syscall_futex_wait(syscall_reg_t *r);             // 66    uint32_t *futex, uint32_t expected
void syscall_start_thread(syscall_reg_t *r);           // 67    void *entry_point, void *stack_addr, void *tcb_pointer
void syscall_exit_thread(syscall_reg_t *r);            // 68
void syscall_map_to_process(syscall_reg_t *r);         // 69    int pid, uint64_t size
void syscall_core_count(syscall_reg_t *r);             // 70
void syscall_get_core_performance(syscall_reg_t *r);   // 71    cpu_performance_t *out, uint8_t core
void syscall_ms_sleep(syscall_reg_t *r);               // 72    uint64_t ms
void syscall_set_fs(syscall_reg_t *r);                 // 300   uint64_t fs

/* Meme syscalls (very temporary) */
void syscall_print_num(syscall_reg_t *r);

/* Empty syscall (stub for an unimplemented syscall) */
void syscall_empty(syscall_reg_t *r);

void init_syscalls();

extern uint64_t memcpy_from_userspace(void *dst, void *src, uint64_t byte_count);
extern uint64_t strcpy_from_userspace(char *dst, char *src);
extern uint64_t strlen_from_userspace(char *dst);

#endif