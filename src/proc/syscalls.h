#ifndef SYSCALLS_H
#define SYSCALLS_H
#include <stdint.h>

typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8, rsi, rdi, rbp, rdx, rcx, rbx, rax;
} __attribute__((packed)) syscall_reg_t;

void syscall_read(syscall_reg_t *r);
void syscall_write(syscall_reg_t *r);
void syscall_open(syscall_reg_t *r);
void syscall_close(syscall_reg_t *r);
void syscall_seek(syscall_reg_t *r);
void syscall_nanosleep(syscall_reg_t *r);
void syscall_empty(syscall_reg_t *r);

void init_syscalls();

extern uint64_t memcpy_from_userspace(void *dst, void *src, uint64_t byte_count);
extern uint64_t strcpy_from_userspace(char *dst, char *src);
extern uint64_t strlen_from_userspace(char *dst);

#endif