#include "syscalls.h"
#include "mm/vmm.h"
#include "mm/pmm.h"
#include "fs/fd.h"
#include "proc/sleep_queue.h"
#include "proc/scheduler.h"
#include "proc/sched_syscalls.h"
#include "proc/safe_userspace.h"
#include "proc/ipc.h"
#include "sys/smp.h"
#include "sys/apic.h"
#include "klibc/errno.h"
#include "klibc/stdlib.h"
#include "klibc/debug.h"
#include "klibc/logger.h"

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
    register_syscall(8, syscall_seek);
    register_syscall(9, syscall_mmap);
    register_syscall(11, syscall_munmap);
    register_syscall(12, syscall_exit);
    register_syscall(14, syscall_getppid);
    register_syscall(24, syscall_yield);
    register_syscall(35, syscall_nanosleep);
    register_syscall(50, syscall_sprint);
    register_syscall(57, syscall_fork);
    register_syscall(58, syscall_map_from_us_to_process);
    register_syscall(59, syscall_execve);
    register_syscall(60, syscall_ipc_read);
    register_syscall(61, syscall_ipc_write);
    register_syscall(62, syscall_ipc_register);
    register_syscall(63, syscall_ipc_wait);
    register_syscall(64, syscall_ipc_handling_complete);
    register_syscall(65, syscall_futex_wake);
    register_syscall(66, syscall_futex_wait);
    register_syscall(67, syscall_start_thread);
    register_syscall(68, syscall_exit_thread);
    register_syscall(69, syscall_map_to_process);
    register_syscall(70, syscall_core_count);
    register_syscall(71, syscall_get_core_performance);
    register_syscall(72, syscall_ms_sleep);
    register_syscall(300, syscall_set_fs);

    /* Memes */
    register_syscall(123, syscall_print_num);
}

void syscall_handler(syscall_reg_t *r) {
    if (r->rax < (uint64_t) HANDLER_COUNT) {
        //sprintf("Handling syscall: %lu\n", r->rax);
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

void syscall_exit(syscall_reg_t *r) {
    (void) r;
    kill_process(get_cpu_locals()->current_thread->parent_pid); // yeet
}

void syscall_sprint(syscall_reg_t *r) {
#ifdef DEBUG
    char *string = check_and_copy_string((void *) r->rdi);
    sprint(string);
    sprint("\e[0m\n");
    kfree(string);
#else
    (void) r;
#endif
}

void syscall_ipc_read(syscall_reg_t *r) {
    int size = (int) r->rbx;
    if (!range_mapped((void *) r->rdx, size)) {
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
    if (!range_mapped((void *) r->rdx, size)) {
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

void syscall_ipc_wait(syscall_reg_t *r) {
    r->rdx = 0;
    if (!range_mapped((void *) r->rsi, sizeof(syscall_ipc_handle_t))) {
        r->rdx = EFAULT;
        return;
    }

    ipc_handle_t *handle = wait_ipc((int) r->rdi);
    if (!handle) {
        r->rdx = EINVAL;
    }

    interrupt_safe_lock(sched_lock);
    process_t *target_process = processes[get_cpu_locals()->current_thread->parent_pid];
    interrupt_safe_unlock(sched_lock);

    /* Map IPC buffer into the process */
    lock(target_process->brk_lock);
    void *map_buffer_addr = (void *) target_process->current_brk;
    target_process->current_brk += ((handle->size + 0x1000 - 1) / 0x1000) * 0x1000;
    unlock(target_process->brk_lock);

    vmm_map(GET_LOWER_HALF(void *, handle->buffer), map_buffer_addr, (handle->size + 0x1000 - 1) / 0x1000, 
        VMM_PRESENT | VMM_WRITE | VMM_USER);

    syscall_ipc_handle_t *userspace_handle_buf = (void *) r->rsi;
    userspace_handle_buf->original_buffer = handle;
    userspace_handle_buf->operation_type = handle->operation_type;
    userspace_handle_buf->pid = handle->pid;
    userspace_handle_buf->size = handle->size;
    userspace_handle_buf->buffer = map_buffer_addr;
}

void syscall_ipc_handling_complete(syscall_reg_t *r) {
    r->rdx = 0;
    if (!range_mapped((void *) r->rdi, sizeof(syscall_ipc_handle_t))) {
        r->rdx = EFAULT;
        return;
    }
    syscall_ipc_handle_t *handle = (void *) r->rdi;
    handle->original_buffer->err = handle->err;
    handle->original_buffer->size = handle->size;
    trigger_event(handle->original_buffer->ipc_completed); // this should be safer but alas, no
}

void syscall_ipc_register(syscall_reg_t *r) {
    r->rdx = register_ipc_handle((int) r->rdi);
}

void syscall_core_count(syscall_reg_t *r) {
    r->rdx = 0;
    r->rax = cpu_vector.items_count;
}

typedef struct {
    uint64_t time_active;
    uint64_t time_idle;
} __attribute__((packed)) cpu_performance_t;

void syscall_get_core_performance(syscall_reg_t *r) {
    if (!range_mapped((void *) r->rdi, sizeof(cpu_performance_t))) {
        r->rdx = EFAULT;
        return;
    }

    if ((uint8_t) r->rsi > cpu_vector.items_count - 1) {
        r->rdx = EINVAL;
        return;
    }

    r->rdx = 0;

    cpu_performance_t performace;
    cpu_locals_t *locals = hashmap_get_elem(cpu_locals_list, (uint8_t) r->rsi);

    performace.time_active = locals->active_tsc_count;
    performace.time_idle = locals->idle_tsc_count;

    memcpy((uint8_t *) &performace, (uint8_t *) r->rdi, sizeof(cpu_performance_t));
}

void syscall_ms_sleep(syscall_reg_t *r) {
    sleep_ms(r->rdi);
}

void syscall_futex_wake(syscall_reg_t *r) {
    r->rdx = 0;

    void *futex_phys = virt_to_phys((void *) r->rdi, (pt_t *) get_cpu_locals()->current_thread->regs.cr3);
    if ((uint64_t) futex_phys == 0xFFFFFFFFFFFFFFFF) {
        r->rdx = EFAULT;
    }

    r->rdx = futex_wake(futex_phys);
}

void syscall_futex_wait(syscall_reg_t *r) {
    r->rdx = 0;

    void *futex_phys = virt_to_phys((void *) r->rdi, (pt_t *) get_cpu_locals()->current_thread->regs.cr3);
    if ((uint64_t) futex_phys == 0xFFFFFFFFFFFFFFFF) {
        r->rdx = EFAULT;
    }

    r->rdx = futex_wait(futex_phys, (uint32_t) r->rsi);
}

void syscall_start_thread(syscall_reg_t *r) {
    thread_t *new_thread = create_thread(get_cpu_locals()->current_thread->name, (void *) r->rdi, r->rsi, 3);
    new_thread->regs.fs = r->rdx;
    r->rax = add_new_child_thread_no_stack_init(new_thread, get_cpu_locals()->current_thread->parent_pid);
}

void syscall_exit_thread(syscall_reg_t *r) {
    (void) r;
    kill_task(get_cpu_locals()->current_thread->tid);
}

void syscall_map_to_process(syscall_reg_t *r) {
    interrupt_safe_lock(sched_lock);
    if (r->rdi >= process_count) {
        interrupt_safe_unlock(sched_lock);
        r->rdx = 0;
        return;
    }
    process_t *process = processes[r->rdi];
    interrupt_safe_unlock(sched_lock);

    uint64_t pages = (r->rsi + 0x1000 - 1) / 0x1000;

    lock(process->brk_lock);
    void *mapped_addr = (void *) process->current_brk;
    process->current_brk += pages * 0x1000;
    unlock(process->brk_lock);

    void *phys = pmm_alloc(pages * 0x1000);
    memset(GET_HIGHER_HALF(void *, phys), 0, pages * 0x1000);

    vmm_map_pages(phys, mapped_addr, (void *) process->cr3, pages,
        VMM_PRESENT | VMM_USER | VMM_WRITE);
    
    r->rdx = (uint64_t) mapped_addr;
    return;
}

int times_called = 0;
void syscall_map_from_us_to_process(syscall_reg_t *r) {
    times_called++;
    if (!range_mapped((void *) r->rsi, r->rdx)) {
        r->rdx = 0;
        return;
    }

    void *phys = virt_to_phys((void *) (r->rsi & ~(0xfff)), (pt_t *) get_cpu_locals()->current_thread->regs.cr3);
    if (!range_mapped_in_userspace(phys, r->rdx)) {
        r->rdx = 0;
        return;
    }

    uint64_t start = r->rsi & ~(0xfff);
    uint64_t end = start + (((r->rdx + 0x1000 - 1) / 0x1000) * 0x1000);

    interrupt_safe_lock(sched_lock);
    if (r->rdi >= process_list_size) {
        interrupt_safe_unlock(sched_lock);
        r->rdx = 0;
        return;
    }

    /* The process we are mapping to is likely unscheduled, this function is meant to be used in IPC code */
    process_t *process = processes[r->rdi];
    interrupt_safe_unlock(sched_lock);

    uint64_t pages = (end - start) / 0x1000;

    lock(process->brk_lock);
    void *mapped_addr = (void *) process->current_brk;
    process->current_brk += pages * 0x1000;
    unlock(process->brk_lock);

    vmm_map_pages(phys, mapped_addr, (void *) process->cr3, pages,
        VMM_PRESENT | VMM_USER | VMM_WRITE);
    
    r->rdx = (uint64_t) mapped_addr + (r->rsi & 0xfff);

    if (times_called == 2) {
        set_local_watchpoint((void *) r->rsi, 0);

        process->local_watchpoint1 = ((uint64_t) mapped_addr + (r->rsi & 0xfff));
        process->local_watchpoint1_active = 1;
    }
    return;
}

void syscall_empty(syscall_reg_t *r) {
    sprintf("[DripOS] Warning! Unimplemented syscall %lu!\n", r->rax);
    r->rdx = ENOSYS;
}