#include "syscalls.h"
#include "fs/fd.h"
#include "proc/sleep_queue.h"
#include "klibc/errno.h"
#include "klibc/stdlib.h"

#include "drivers/serial.h"

int HANDLER_COUNT = 0;
typedef void (*syscall_handler_t)(syscall_reg_t *r);

syscall_handler_t *syscall_handlers = 0;

void register_syscall(int num, syscall_handler_t handler) {
    if (HANDLER_COUNT < num+1) {
        syscall_handlers = krealloc(syscall_handlers, sizeof(syscall_handler_t)*(num+1));
        HANDLER_COUNT = num+1;
        for (int i = 0; i < HANDLER_COUNT; i++) {
            if (!syscall_handlers[i]) {
                syscall_handlers[i] = syscall_empty;
            }
        }
    }

    syscall_handlers[num] = handler;
}

void init_syscalls() {
    /* Useful */
    register_syscall(0, syscall_read);
    register_syscall(1, syscall_write);
    register_syscall(2, syscall_open);
    register_syscall(3, syscall_close);
    register_syscall(8, syscall_seek);
    register_syscall(9, syscall_mmap);
    register_syscall(11, syscall_munmap);
    register_syscall(24, syscall_yield);
    register_syscall(35, syscall_nanosleep);
    register_syscall(57, syscall_fork);

    /* Memes */
    register_syscall(123, syscall_print_num);
}

void syscall_handler(syscall_reg_t *r) {
    //sprintf("\nGot syscall with rax = %lx", r->rax);
    if (r->rax < (uint64_t) HANDLER_COUNT) {
        get_thread_locals()->errno = 0; // Clear errno
        syscall_handlers[r->rax](r);
    }
}

void syscall_nanosleep(syscall_reg_t *r) {
    r->rax = nanosleep((struct timespec *) r->rdi, (struct timespec *) r->rsi);
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

void syscall_mmap(syscall_reg_t *r) {
    r->rax = (uint64_t) psuedo_mmap((void *) r->rdi, r->rsi); 
}

void syscall_munmap(syscall_reg_t *r) {
    r->rax = (uint64_t) munmap((void *) r->rdi, r->rsi);
}

void syscall_yield(syscall_reg_t *r) {
    (void) r;
    yield();
}

void syscall_fork(syscall_reg_t *r) {
    (void) r;
    fork();
}

void syscall_print_num(syscall_reg_t *r) {
    sprintf("\nPRINTING NUMBER FROM SYSCALL: %lu", r->rdi);
    r->rax = 0;
}

void syscall_empty(syscall_reg_t *r) {
    sprintf("\n[DripOS] Warning! Unimplemented syscall %lu!", r->rax);
    r->rax = -ENOSYS;
    get_thread_locals()->errno = -ENOSYS;
}