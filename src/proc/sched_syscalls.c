#include "sched_syscalls.h"
#include "urm.h"
#include "safe_userspace.h"
#include "sys/smp.h"
#include "mm/vmm.h"
#include "mm/pmm.h"
#include "klibc/errno.h"
#include "klibc/stdlib.h"
#include "io/msr.h"
#include <stddef.h>

typedef struct {
    thread_t **waiting;
    uint32_t count;
} futex_wait_list_t;
hashmap_t *futex_waiters = (void *) 0; // a hashmap of futex address -> thread **
lock_t futex_lock = {0, 0, 0, 0};

void *psuedo_mmap(void *base, uint64_t len, syscall_reg_t *r) {
    interrupt_safe_lock(sched_lock);
    len = (len + 0x1000 - 1) / 0x1000;
    process_t *process = processes[get_cpu_locals()->current_thread->parent_pid];
    if (!process) { r->rdx = ESRCH; interrupt_safe_unlock(sched_lock); return (void *) 0; } // bruh

    void *phys = pmm_alloc(len * 0x1000);
    memset(GET_HIGHER_HALF(void *, phys), 0, len * 0x1000);

    if (base) {
        uint64_t mapped = 0;
        int map_err = 0;
        for (uint64_t i = 0; i < len; i++) {
            int err = vmm_map_pages(phys, (void *) ((uint64_t) base + i * 0x1000), 
                (void *) process->cr3, 1, 
                VMM_WRITE | VMM_USER | VMM_PRESENT);
            phys = (void *) ((uint64_t) phys + 0x1000);
            
            if (err) {
                mapped = i;
                map_err = 1;
                break;
            }
        }

        if (map_err) {
            /* Unmap the error */
            for (uint64_t i = 0; i < mapped; i++) {
                vmm_unmap_pages((void *) ((uint64_t) base + i * 0x1000), 
                    (void *) process->cr3, 1);
            }
            phys = (void *) ((uint64_t) phys - (0x1000 * mapped));
            r->rdx = ENOMEM;
            pmm_unalloc(phys, len * 0x1000);

            interrupt_safe_unlock(sched_lock);
            return (void *) 0;
        } else {
            void *ret = (void *) base;

            interrupt_safe_unlock(sched_lock);
            return ret;
        }
    } else {
        lock(process->brk_lock);
        uint64_t mapped = 0;
        int map_err = 0;
        for (uint64_t i = 0; i < len; i++) {
            int err = vmm_map_pages(phys, (void *) ((uint64_t) process->current_brk + i * 0x1000), 
                (void *) process->cr3, 1, 
                VMM_WRITE | VMM_USER | VMM_PRESENT);
            
            phys = (void *) ((uint64_t) phys + 0x1000);
            
            if (err) {
                mapped = i;
                map_err = 1;
                break;
            }
        }

        if (map_err) {
            /* Unmap the error */
            for (uint64_t i = 0; i < mapped; i++) {
                vmm_unmap_pages((void *) ((uint64_t) process->current_brk + i * 0x1000), 
                    (void *) process->cr3, 1);
            }
            r->rdx = ENOMEM;
            phys = (void *) ((uint64_t) phys - (0x1000 * mapped));
            pmm_unalloc(phys, len * 0x1000);

            unlock(process->brk_lock);

            interrupt_safe_unlock(sched_lock);
            return (void *) 0;
        } else {
            void *ret = (void *) process->current_brk;
            process->current_brk += len * 0x1000;

            unlock(process->brk_lock);

            interrupt_safe_unlock(sched_lock);
            return ret;
        }
    }
}

int munmap(char *addr, uint64_t len) {
    interrupt_safe_lock(sched_lock);
    len = (len + 0x1000 - 1) / 0x1000;
    process_t *process = processes[get_cpu_locals()->current_thread->parent_pid];
    interrupt_safe_unlock(sched_lock);
    if (!process) { return -ESRCH; } // bruh

    if ((uint64_t) addr != ((uint64_t) addr & ~(0xfff))) {
        return -EINVAL;
    }
    if ((uint64_t) addr > 0x7fffffffffff) {
        // Not user memory, stupid userspace
        return -EINVAL;
    }

    for (uint64_t i = 0; i < len; i++) {
        void *phys = virt_to_phys(addr, (void *) vmm_get_base());
        vmm_unmap(addr, 1);
        if ((uint64_t) phys != 0xffffffffffffffff) {
            pmm_unalloc(phys, 0x1000);
        }
        addr += 0x1000;
    }
    return 0;
}

int fork(syscall_reg_t *r) {
    sprintf("got fork call with r = %lx\n", r);
    interrupt_safe_lock(sched_lock);
    process_t *process = processes[get_cpu_locals()->current_thread->parent_pid]; // Old process
    void *new_cr3 = vmm_fork((void *) process->cr3); // Fork address space

    process_t *forked_process = create_process(process->name, new_cr3);
    int64_t new_pid = -1;
    for (uint64_t i = 0; i < process_list_size; i++) {
        if (!processes[i]) {
            new_pid = i;
            break;
        }
    }
    if (new_pid == -1) {
        processes = krealloc(processes, (process_list_size + 10) * sizeof(process_t *));
        new_pid = process_list_size;
        process_list_size += 10;
    }
    processes[new_pid] = forked_process;
    forked_process->pid = new_pid;

    process_t *new_process = processes[new_pid]; // Get the process struct
    int64_t old_pid = process->pid;
    interrupt_safe_unlock(sched_lock);

    /* Copy fds */
    clone_fds(old_pid, new_pid);

    interrupt_safe_lock(sched_lock);

    /* Parent id */
    new_process->ppid = process->pid;

    /* Other ids */
    new_process->uid = process->uid;
    new_process->gid = process->gid;
    
    /* Other data about the process */
    lock(process->brk_lock);
    new_process->current_brk = process->current_brk;
    unlock(process->brk_lock);

    /* New thread */
    thread_t *old_thread = get_cpu_locals()->current_thread;

    thread_t *thread = create_thread(old_thread->name, (void (*)()) old_thread->regs.rip, old_thread->regs.rsp, old_thread->ring);
    thread->regs.rax = 0;
    thread->regs.rbx = r->rbx;
    thread->regs.rcx = r->rcx;
    thread->regs.rdx = r->rdx;
    thread->regs.rbp = r->rbp;
    thread->regs.rsi = r->rsi;
    thread->regs.rdi = r->rdi;
    thread->regs.r8 = r->r8;
    thread->regs.r9 = r->r9;
    thread->regs.r10 = r->r10;
    thread->regs.r11 = r->r11;
    thread->regs.r12 = r->r12;
    thread->regs.r13 = r->r13;
    thread->regs.r14 = r->r14;
    thread->regs.r15 = r->r15;
    thread->regs.rsp = get_cpu_locals()->thread_user_stack;
    thread->regs.rip = r->rcx;
    thread->regs.rflags = r->r11;
    memcpy((uint8_t *) old_thread->sse_region, (uint8_t *) thread->sse_region, 512);

    interrupt_safe_unlock(sched_lock);

    add_new_child_thread_no_stack_init(thread, new_pid); // Add the thread

    /* Bye bye */
    return new_pid;
}

void execve(char *executable_path, char **argv, char **envp, syscall_reg_t *r) {
    r->rdx = 0;
    uint64_t argc = 0;
    uint64_t envc = 0;
    int found_null_argv = 0;
    int found_null_envp = 0;

    for (uint64_t i = 0; i < 128; i++) {
        if (!range_mapped((void *) ((uint64_t) argv + (sizeof(char *) * i)), sizeof(char *)) || ((uint64_t) argv + 8 + (sizeof(char *) * i)) > 0x7fffffffffff) {
            r->rdx = EFAULT;
            return;
        }
        if (!argv[i]) {
            found_null_argv = 1;
            break;
        }
        argc++;
    }

    for (uint64_t i = 0; i < 128; i++) {
        if (!range_mapped((void *) ((uint64_t) envp + (sizeof(char *) * i)), sizeof(char *)) || ((uint64_t) envp + 8 + (sizeof(char *) * i)) > 0x7fffffffffff) {
            r->rdx = EFAULT;
            return;
        }
        if (!envp[i]) {
            found_null_envp = 1;
            break;
        }
        envc++;
    }

    if (!found_null_argv || !found_null_envp) {
        r->rdx = EFAULT;
        return;
    }

    char **kernel_argv = kcalloc(sizeof(char *) * argc);
    char **kernel_envp = kcalloc(sizeof(char *) * envc);
    char *kernel_exec_path = (void *) 0;

    for (uint64_t i = 0; i < argc; i++) {
        char *string = argv[i];
        void *kernel_string = check_and_copy_string(string);
        if (!kernel_string) {
            goto fault_return;
        }

        kernel_argv[i] = kernel_string;
    }

    for (uint64_t i = 0; i < envc; i++) {
        char *string = envp[i];
        void *kernel_string = check_and_copy_string(string);
        if (!kernel_string) {
            goto fault_return;
        }

        kernel_envp[i] = kernel_string;
    }

    kernel_exec_path = check_and_copy_string(executable_path);
    if (!kernel_exec_path) {
        goto fault_return;
    }
    urm_execve_data data;
    data.argv = kernel_argv;
    data.envp = kernel_envp;
    data.executable_path = kernel_exec_path;
    data.envc = envc;
    data.argc = argc;
    data.pid = get_cpu_locals()->current_thread->parent_pid;
    data.tid = get_cpu_locals()->current_thread->tid;
    int ret = send_urm_request(&data, URM_EXECVE);
    goto done;

fault_return:
    for (uint64_t x = 0; x < argc; x++) {
        if (kernel_argv[x]) {
            kfree(kernel_argv[x]);
        }
    }
    kfree(kernel_argv);
    for (uint64_t x = 0; x < envc; x++) {
        if (kernel_envp[x]) {
            kfree(kernel_envp[x]);
        }
    }
    kfree(kernel_envp);
    if (kernel_exec_path) {
        kfree(kernel_exec_path);
    }
    r->rdx = EFAULT;
    return;
done:
    sprintf("returning\n");
    for (uint64_t x = 0; x < argc; x++) {
        if (kernel_argv[x]) {
            kfree(kernel_argv[x]);
        }
    }
    kfree(kernel_argv);
    for (uint64_t x = 0; x < envc; x++) {
        if (kernel_envp[x]) {
            kfree(kernel_envp[x]);
        }
    }
    kfree(kernel_envp);
    if (kernel_exec_path) {
        kfree(kernel_exec_path);
    }
    r->rdx = ret; // return error :(
}

void set_fs_base_syscall(uint64_t base) {
    get_cpu_locals()->current_thread->regs.fs = base;
    write_msr(0xC0000100, get_cpu_locals()->current_thread->regs.fs); // Set the FS base in case the scheduler hasn't rescheduled
}

/* Wake a single thread that is waiting on the futex */
int futex_wake(uint32_t *futex) {
    lock(futex_lock);
    if (!futex_waiters) {
        unlock(futex_lock);
        return 0;
    }

    futex_wait_list_t *waiting_threads = hashmap_get_elem(futex_waiters, (uint64_t) futex);
    if (!waiting_threads) { // no one is waiting
        unlock(futex_lock);
        return 0;
    }

    for (uint32_t i = 0; i < waiting_threads->count; i++) {
        if (waiting_threads->waiting[i]) {
            interrupt_safe_lock(sched_lock);
            waiting_threads->waiting[i]->state = READY;
            interrupt_safe_unlock(sched_lock);

            waiting_threads->waiting[i] = NULL;
            break;
        }
    }

    unlock(futex_lock);
    return 0;
}

int futex_wait(uint32_t *futex, uint32_t expected_value) {
    uint32_t *higher_half_futex = GET_HIGHER_HALF(void *, futex);
    lock(futex_lock);
    if (*higher_half_futex != expected_value) {
        unlock(futex_lock);
        return 1;
    }

    if (!futex_waiters) {
        futex_waiters = init_hashmap();
    }
    
    futex_wait_list_t *waiting_threads = hashmap_get_elem(futex_waiters, (uint64_t) futex);
    if (!waiting_threads) {
        waiting_threads = kcalloc(sizeof(futex_wait_list_t));
        hashmap_set_elem(futex_waiters, (uint64_t) futex, waiting_threads);
    }

    interrupt_safe_lock(sched_lock);
    thread_t *cur_thread = get_cpu_locals()->current_thread;
    int64_t index = -1;
    for (uint32_t i = 0; i < waiting_threads->count; i++) {
        if (!waiting_threads->waiting[i]) {
            index = i;
            break;
        }
    }

    if (index == -1) {
        index = waiting_threads->count++;
        waiting_threads->waiting = krealloc(waiting_threads->waiting, waiting_threads->count * sizeof(thread_t *));
    }

    waiting_threads->waiting[index] = cur_thread;
    cur_thread->state = BLOCKED;
    
    unlock(futex_lock);
    force_unlocked_schedule();
    return 0;
}