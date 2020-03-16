#include "syscalls.h"
#include "fs/fd.h"

#include "drivers/serial.h"

#define HANDLER_COUNT 3
typedef void (*syscall_handler_t)(syscall_reg_t *r);

syscall_handler_t syscall_handlers[] = {syscall_read, syscall_write, syscall_open};

void syscall_handler(syscall_reg_t *r) {
    sprintf("\nGot syscall with rax = %lx", r->rax);
    if (r->rax < HANDLER_COUNT) {
        syscall_handlers[r->rax](r);
    }
}

void syscall_read(syscall_reg_t *r) {
    r->rax = fd_read((int) r->rdi, (void *) r->rsi, r->rdx);
}

void syscall_write(syscall_reg_t *r) {
    r->rax = fd_write((int) r->rdi, (void *) r->rsi, r->rdx);
}

void syscall_open(syscall_reg_t *r) {
    r->rax = fd_open((char *) r->rdi, (int) r->rsi);
}

void syscall_close(syscall_reg_t *r) {
    r->rax = fd_close((int) r->rdi);
}