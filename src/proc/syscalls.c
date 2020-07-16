#include "syscalls.h"
#include "mm/vmm.h"
#include "fs/fd.h"
#include "proc/sleep_queue.h"
#include "proc/scheduler.h"
#include "proc/safe_userspace.h"
#include "proc/ipc.h"
#include "sys/smp.h"
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
    register_syscall(5, syscall_getpid);
    register_syscall(6, syscall_sleepms);
    register_syscall(8, syscall_seek);
    register_syscall(9, syscall_mmap);
    register_syscall(11, syscall_munmap);
    register_syscall(12, syscall_exit);
    register_syscall(14, syscall_getppid);
    register_syscall(24, syscall_yield);
    register_syscall(35, syscall_nanosleep);
    register_syscall(50, syscall_sprint);
    register_syscall(57, syscall_fork);
    register_syscall(59, syscall_execve);
    register_syscall(60, syscall_ipc_read);
    register_syscall(61, syscall_ipc_write);
    register_syscall(300, syscall_set_fs);

    /* Memes */
    register_syscall(123, syscall_print_num);
}

void syscall_handler(syscall_reg_t *r) {
    if (r->rax < (uint64_t) HANDLER_COUNT) {
        sprintf("Handling syscall: %lu\n", r->rax);
        syscall_handlers[r->rax](r);
    }
}

void syscall_nanosleep(syscall_reg_t *r) {
    r->rax = nanosleep((struct timespec *) r->rdi, (struct timespec *) r->rsi);
}

void syscall_read(syscall_reg_t *r) {
    int ret = fd_read((int) r->rdi, (void *) r->rsi, r->rdx);
    if (ret >= 0) {
        r->rax = ret;
    } else {
        r->rax = -1;
        r->rdx = -ret;
    }
}

void syscall_write(syscall_reg_t *r) {
    int ret = fd_write((int) r->rdi, (void *) r->rsi, r->rdx);
    if (ret >= 0) {
        r->rax = ret;
    } else {
        r->rax = -1;
        r->rdx = -ret;
    }
}

void syscall_open(syscall_reg_t *r) {
    int ret = fd_open((char *) r->rdi, (int) r->rsi);
    if (ret >= 0) {
        r->rax = ret;
    } else {
        r->rax = -1;
        r->rdx = -ret;
    }
    //sprintf("ret: %d\n", ret);
}

void syscall_close(syscall_reg_t *r) {
    int ret = fd_close((int) r->rdi);
    if (ret >= 0) {
        r->rax = ret;
    } else {
        r->rax = -1;
        r->rdx = -ret;
    }
}

void syscall_seek(syscall_reg_t *r) {
    //sprintf("seek: %lu, whence: %d\n", r->rsi, (int) r->rdx);
    int64_t ret = fd_seek((int) r->rdi, (uint64_t) r->rsi, (int) r->rdx);
    if (ret >= 0) {
        r->rax = ret;
    } else {
        r->rax = -1;
        r->rdx = -ret;
    }
}

void syscall_mmap(syscall_reg_t *r) {
    //sprintf("Base: %lx, Size: %lx.\n", r->rdi, r->rsi);
    r->rax = (uint64_t) psuedo_mmap((void *) r->rdi, r->rsi, r);
    //sprintf("Returning %lx\n", r->rax);
}

void syscall_munmap(syscall_reg_t *r) {
    int ret = munmap((void *) r->rdi, r->rsi);
    if (ret == 0) {
        r->rax = ret;
    } else {
        r->rax = 1;
        r->rdx = -ret;
    }
}

void syscall_yield(syscall_reg_t *r) {
    (void) r;
    yield();
}

void syscall_fork(syscall_reg_t *r) {
    r->rax = fork(r);
}

void syscall_execve(syscall_reg_t *r) {
    execve((char *) r->rdi, (char **) r->rsi, (char **) r->rdx, r);
}

void syscall_print_num(syscall_reg_t *r) {
    sprintf("PRINTING NUMBER FROM SYSCALL: %lu\n", r->rdi);
    r->rax = 0;
}

void syscall_set_fs(syscall_reg_t *r) {
    set_fs_base_syscall(r->rdi);
    r->rax = 0;
}

void syscall_getpid(syscall_reg_t *r) {
    (void) r;
    r->rax = get_cpu_locals()->current_thread->parent_pid;
}

void syscall_getppid(syscall_reg_t *r) {
    (void) r;
    interrupt_safe_lock(sched_lock);
    process_t *process = processes[get_cpu_locals()->current_thread->parent_pid];
    interrupt_safe_unlock(sched_lock);
    r->rax = process->ppid;
}

void syscall_sleepms(syscall_reg_t *r) {
    sleep_ms(r->rdi);
}

void syscall_exit(syscall_reg_t *r) {
    (void) r;
    kill_process(get_cpu_locals()->current_thread->parent_pid); // yeet
}

void syscall_sprint(syscall_reg_t *r) {
    char *string = check_and_copy_string((void *) r->rdi);
    sprint(string);
    sprint("\e[0m\n");
    kfree(string);
}

void syscall_ipc_read(syscall_reg_t *r) {
    int size = (int) r->rbx;
    if (!range_mapped(r->rdx, size)) {
        union ipc_err err;
        err.parts.err = IPC_BUFFER_INVALID;
        r->rdx = err.real_err;
        return;
    }

    void *buffer = kcalloc(size);
    void *userspace_addr = (void *) r->rdx;
    r->rdx = read_ipc_server((int) r->rdi, (int) r->rsi, buffer, size).real_err; // Set err
    memcpy(buffer, userspace_addr, size); // Copy the buffer in case anything was read
    kfree(buffer);
}

void syscall_ipc_write(syscall_reg_t *r) {
    int size = (int) r->rbx;
    if (!range_mapped(r->rdx, size)) {
        union ipc_err err;
        err.parts.err = IPC_BUFFER_INVALID;
        r->rdx = err.real_err;
        return;
    }

    void *buffer = kcalloc(size);
    void *userspace_addr = (void *) r->rdx;
    memcpy(userspace_addr, buffer, size); // Copy the buffer for writing
    r->rdx = write_ipc_server((int) r->rdi, (int) r->rsi, buffer, size).real_err; // Set err
    kfree(buffer);
}

void syscall_empty(syscall_reg_t *r) {
    sprintf("[DripOS] Warning! Unimplemented syscall %lu!\n", r->rax);
    r->rdx = ENOSYS;
}