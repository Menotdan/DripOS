#include "syscalls.h"
#include "fs/fd.h"
#include "proc/scheduler.h"
#include "klibc/errno.h"

#include "drivers/serial.h"

#define HANDLER_COUNT 9
typedef void (*syscall_handler_t)(syscall_reg_t *r);

syscall_handler_t syscall_handlers[] = {syscall_read, syscall_write, syscall_open, syscall_close, syscall_empty, 
    syscall_empty, syscall_empty, syscall_empty, syscall_seek};

void syscall_handler(syscall_reg_t *r) {
    sprintf("\nGot syscall with rax = %lx", r->rax);
    if (r->rax < HANDLER_COUNT) {
        get_thread_locals()->errno = 0; // Clear errno
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

void syscall_seek(syscall_reg_t *r) {
    r->rax = fd_seek((int) r->rdi, (uint64_t) r->rsi, (int) r->rdx);
}

void syscall_empty(syscall_reg_t *r) {
    sprintf("\n[DripOS] Warning! Unimplemented syscall %lu!", r->rax);
    r->rax = -ENOSYS;
    get_thread_locals()->errno = -ENOSYS;
}