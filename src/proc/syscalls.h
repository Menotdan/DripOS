#ifndef SYSCALLS_H
#define SYSCALLS_H
#include <stdint.h>

typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8, rsi, rdi, rbp, rdx, rcx, rbx, rax;
} __attribute__((packed)) syscall_reg_t;

void syscall_read(syscall_reg_t *r);
void syscall_write(syscall_reg_t *r);
void syscall_open(syscall_reg_t *r);

#endif