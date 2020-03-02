#include "syscalls.h"
#include "fs/fd.h"

#define HANDLER_COUNT 1
typedef void (*syscall_handler_t)(syscall_reg_t *r);

syscall_handler_t syscall_handlers[] = {syscall_read};

void syscall_handler(syscall_reg_t *r) {
    if (r->rax < HANDLER_COUNT) {
        syscall_handlers[r->rax](r);
    }
}

void syscall_read(syscall_reg_t *r) {
    fd_read((int) r->rdi, (void *) r->rsi, r->rdx);
}