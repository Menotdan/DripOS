#include "scheduler.h"
#include "klibc/dynarray.h"
#include "klibc/stdlib.h"
#include "klibc/string.h"
#include "klibc/queue.h"
#include "klibc/errno.h"
#include "drivers/tty/tty.h"
#include "drivers/serial.h"
#include "io/msr.h"
#include "mm/vmm.h"
#include "mm/pmm.h"
#include "sys/smp.h"
#include "sys/apic.h"

#include "drivers/pit.h"

extern char syscall_stub[];

dynarray_t tasks;
dynarray_t processes;

uint8_t scheduler_enabled = 0;
lock_t scheduler_lock = {0, 0, 0, 0};
int cpu_holding_sched_lock = -1; // -1 means unknown/none

task_regs_t default_kernel_regs = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0x10,0x8,0,0x202,0};
task_regs_t default_user_regs = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0x23,0x1B,0,0x202,0};

uint64_t get_thread_list_size() {
    return tasks.array_size;
}

void *get_thread_elem(uint64_t elem) {
    return dynarray_getelem(&tasks, elem);
}

void ref_thread_elem(uint64_t elem) {
    dynarray_getelem(&tasks, elem);
}

void unref_thread_elem(uint64_t elem) {
    dynarray_unref(&tasks, elem);
}

void lock_scheduler() {
    lock(scheduler_lock);
}

void unlock_scheduler() {
    unlock(scheduler_lock);
}

thread_info_block_t *get_thread_locals() {
    thread_info_block_t *ret;
    asm volatile("movq %%fs:(0), %0;" : "=r"(ret));
    return ret;
}

void send_scheduler_ipis() {
    madt_ent0_t **cpus = (madt_ent0_t **) vector_items(&cpu_vector);
    for (uint64_t i = 0; i < cpu_vector.items_count; i++) {
        if (i < cpu_vector.items_count) {
            if ((cpus[i]->cpu_flags & 1 || cpus[i]->cpu_flags & 2) && cpus[i]->apic_id != get_lapic_id()) {
                send_ipi(cpus[i]->apic_id, (1 << 14) | 253); // Send interrupt 253
            }
        }
    }
}

void start_idle() {
    get_cpu_locals()->idle_start_tsc = read_tsc();
}

void end_idle() {
    cpu_locals_t *cpu_locals = get_cpu_locals();
    cpu_locals->idle_end_tsc = read_tsc();
    cpu_locals->idle_tsc_count += (cpu_locals->idle_end_tsc - cpu_locals->idle_start_tsc);
}

void _idle() {
    while (1) {
        asm volatile("hlt");
    }
}

/* Initialize the BSP for scheduling */
void scheduler_init_bsp() {
    tasks.array_size = 0;
    tasks.base = 0;
    processes.array_size = 0;
    processes.base = 0;

    /* Setup syscall MSRs for this CPU */
    write_msr(0xC0000081, read_msr(0xC0000081) | ((uint64_t) 0x8 << 32));
    write_msr(0xC0000081, read_msr(0xC0000081) | ((uint64_t) 0x18 << 48));
    write_msr(0xC0000082, (uint64_t) syscall_stub); // Start execution at the syscall stub when a syscall occurs
    write_msr(0xC0000084, 0); // Mask nothing
    write_msr(0xC0000080, read_msr(0xC0000080) | 1); // Set the syscall enable bit

    /* Setup the idle thread */
    uint64_t idle_rsp = (uint64_t) kcalloc(0x1000) + 0x1000;
    task_t *new_idle = create_thread("Idle thread", _idle, idle_rsp, 0);
    new_idle->state = BLOCKED; // Idle thread will *not* run unless provoked
    int64_t idle_tid = start_thread(new_idle);

    get_cpu_locals()->idle_tid = idle_tid;
    sprintf("\nIdle task: %ld", get_cpu_locals()->idle_tid);
}

/* Initilialize an AP for scheduling */
void scheduler_init_ap() {
    /* Setup syscall MSRs for this CPU */
    write_msr(0xC0000081, read_msr(0xC0000081) | ((uint64_t) 0x8 << 32));
    write_msr(0xC0000081, read_msr(0xC0000081) | ((uint64_t) 0x18 << 48));
    write_msr(0xC0000082, (uint64_t) syscall_stub); // Start execution at the syscall stub when a syscall occurs
    write_msr(0xC0000084, 0);
    write_msr(0xC0000080, read_msr(0xC0000080) | 1); // Set the syscall enable bit

    /* Setup the idle thread */
    uint64_t idle_rsp = (uint64_t) kcalloc(0x1000) + 0x1000;
    task_t *new_idle = create_thread("Idle thread", _idle, idle_rsp, 0);
    new_idle->state = BLOCKED; // Idle thread will *not* run unless provoked
    int64_t idle_tid = start_thread(new_idle);

    get_cpu_locals()->idle_tid = idle_tid;
    sprintf("\nIdle task: %ld", get_cpu_locals()->idle_tid);
}

/* Create a new thread *and* add it to the dynarray */
int64_t new_thread(char *name, void (*main)(), uint64_t rsp, int64_t pid, uint8_t ring) {
    task_t *new_task = create_thread(name, main, rsp, ring);
    int64_t new_tid = add_new_child_thread(new_task, pid);
    return new_tid;
}

/* Add the thread to the dynarray */
int64_t start_thread(task_t *thread) {
    lock(scheduler_lock);
    cpu_holding_sched_lock = get_cpu_locals()->cpu_index;

    int64_t tid = dynarray_add(&tasks, thread, sizeof(task_t));
    kfree(thread);
    thread = dynarray_getelem(&tasks, tid);
    thread->tid = tid;
    dynarray_unref(&tasks, tid);

    cpu_holding_sched_lock = -1;
    unlock(scheduler_lock);
    return tid;
}

/* Allocate data for a new thread data block and return it */
task_t *create_thread(char *name, void (*main)(), uint64_t rsp, uint8_t ring) {
    /* Allocate new task and it's kernel stack */
    task_t *new_task = kcalloc(sizeof(task_t));
    new_task->kernel_stack = (uint64_t) kcalloc(0x1000) + 0x1000;

    /* Setup ring */
    if (ring == 3) {
        new_task->regs = default_user_regs;
    } else {
        new_task->regs = default_kernel_regs;
    }
    /* Set rip, rsp, cr3, and name */
    new_task->regs.rip = (uint64_t) main;
    new_task->regs.rsp = rsp;
    new_task->ring = ring;
    new_task->regs.cr3 = base_kernel_cr3;
    new_task->regs.fs = (uint64_t) kcalloc(sizeof(thread_info_block_t));
    new_task->sleep_node = kcalloc(sizeof(sleep_queue_t));
    vmm_remap(GET_LOWER_HALF(void *, new_task->regs.fs), (void *) new_task->regs.fs,
        1, VMM_PRESENT | VMM_WRITE | VMM_USER);
    ((thread_info_block_t *) new_task->regs.fs)->meta_pointer = new_task->regs.fs;
    new_task->state = READY;
    strcpy(name, new_task->name);

    return new_task;
}

/* Add a new thread to the dynarray and as a child of a process */
int64_t add_new_child_thread(task_t *task, int64_t pid) {
    lock(scheduler_lock);
    cpu_holding_sched_lock = get_cpu_locals()->cpu_index;

    /* Find the parent process */
    process_t *new_parent = dynarray_getelem(&processes, pid);
    if (!new_parent) { sprintf("\n[Scheduler] Couldn't find parent"); return -1; }

    /* Store the new task and save its TID */
    int64_t new_tid = dynarray_add(&tasks, task, sizeof(task_t));
    task_t *task_item = dynarray_getelem(&tasks, new_tid);
    task_item->tid = new_tid;
    task_item->regs.cr3 = new_parent->cr3; // Inherit parent's cr3
    task_item->parent_pid = pid;

    if (new_parent->threads.array_size == 0) { // No children
        // -1 because kmalloc creates boundaries so page might not be mapped :|
        void *phys = virt_to_phys((void *) (task_item->regs.rsp - 1), (pt_t *) task_item->regs.cr3);
        if ((uint64_t) phys == 0xFFFFFFFFFFFFFFFF) {
            sprintf("\nRSP not mapped :thonk:");
            goto done;
        }
        sprintf("\nvirt: %lx", task_item->regs.rsp);
        sprintf("\nphys: %lx", phys);
        uint64_t stack = GET_HIGHER_HALF(uint64_t, phys) + 1;
        sprintf("\nstack var: %lx", stack);
        uint64_t old_stack = stack;

        int argc = 3;
        char *argv[] = {task_item->name, "hello", "urdum"};

        /* Add argc, argv, and auxv */

        // Actual data
        uint64_t *string_offsets = kcalloc(sizeof(uint64_t) * argc);
        uint64_t total_string_len = 0;
        for (int i = 0; i < argc; i++) {
            total_string_len += (strlen(argv[i]) + 1);
            string_offsets[i] = total_string_len;

            char *new_string = (char *) (stack - string_offsets[i]);
            sprintf("\nString: %lx, string offset: %lu", new_string, string_offsets[i]);
            strcpy(argv[i], new_string);
        }
        stack -= total_string_len;
        stack = stack & ~(0b1111); // align the stack

        uint64_t auxv_count = 1;
        auxv_t *auxv = (auxv_t *) stack - (auxv_count * 16);
        for (uint64_t i = 0; i < auxv_count; i++) {
            auxv[i].a_un.a_val = 0;
            auxv[i].a_type = 0;
        }
        stack -= (auxv_count * 16);
        uint64_t auxv_offset = old_stack - stack; // rdx = Top of stack - auxv_offset

        *(uint64_t *) (stack - 8) = 0;
        stack -= 8;

        uint64_t enviroment_count = 1;
        stack -= enviroment_count * 8;
        char **enviroment = (char **) stack;
        for (uint64_t i = 0; i < enviroment_count; i++) {
            enviroment[i] = 0;
        }

        stack -= 8;
        *(uint64_t *) (stack) = 0;

        stack -= argc * 8;
        for (int i = 0; i < argc; i++) {
            *(uint64_t *) (stack + (i * 8)) = task_item->regs.rsp - string_offsets[i]; // Point to the strings
        }
        uint64_t argv_offset = old_stack - stack; // rsi = Top of stack - argv_offset

        task_item->regs.rdi = (uint64_t) argc;
        task_item->regs.rsi = task_item->regs.rsp - argv_offset;
        task_item->regs.rdx = task_item->regs.rsp - auxv_offset;
        task_item->regs.rsp -= (old_stack - stack);
    }
done:

    dynarray_unref(&tasks, new_tid);
    kfree(task);

    /* Add the TID to it's parent's threads list */
    dynarray_add(&new_parent->threads, &new_tid, sizeof(int64_t));

    cpu_holding_sched_lock = -1;
    unlock(scheduler_lock);
    return new_tid;
}

/* Allocate new data for a process, then return the new PID */
int64_t new_process(char *name, void *new_cr3) {
    lock(scheduler_lock);
    cpu_holding_sched_lock = get_cpu_locals()->cpu_index;

    /* Allocate new process */
    process_t *new_process = kcalloc(sizeof(process_t));
    /* New cr3 */
    new_process->cr3 = (uint64_t) new_cr3;
    /* Default UID and GID */
    new_process->uid = 0;
    new_process->gid = 0;
    new_process->fd_table = kcalloc(10 * sizeof(fd_entry_t));
    new_process->fd_table_size = 10;
    new_process->current_brk = 0x10000000000;
    strcpy(name, new_process->name);

    /* Add element to the dynarray and save the PID */
    int64_t pid = dynarray_add(&processes, (void *) new_process, sizeof(process_t));
    process_t *process_item = dynarray_getelem(&processes, pid);
    process_item->pid = pid;
    dynarray_unref(&processes, pid);

    cpu_holding_sched_lock = -1;
    unlock(scheduler_lock);
    /* Free the old data since it's in the dynarray */
    kfree(new_process);

    /* Open stdout, stdin, and stderr */
    open_remote_fd("/dev/tty1", 0, pid); // stdout
    open_remote_fd("/dev/tty1", 0, pid); // stdin
    open_remote_fd("/dev/tty1", 0, pid); // stderr

    return pid;
}

/* Wrapper for some other parts of the "API" */
void new_kernel_process(char *name, void (*main)()) {
    int64_t task_parent_pid = new_process(name, (void *) base_kernel_cr3);
    uint64_t new_rsp = (uint64_t) kcalloc(TASK_STACK_SIZE) + TASK_STACK_SIZE;
    new_thread(name, main, new_rsp, task_parent_pid, 0);
}

int64_t pick_task() {
    int64_t tid_ret = -1;

    if (!(get_cpu_locals()->current_thread)) {
        return tid_ret;
    }

    /* Prioritze tasks right after the current task */
    for (int64_t t = get_cpu_locals()->current_thread->tid + 1; t < tasks.array_size; t++) {
        task_t *task = dynarray_getelem(&tasks, t);
        if (task) {
            if (task->state == READY) {
                tid_ret = task->tid;
                dynarray_unref(&tasks, t);
                return tid_ret;
            }
        }
        dynarray_unref(&tasks, t);
    }

    /* Then look at the rest of the tasks */
    for (int64_t t = 0; t < get_cpu_locals()->current_thread->tid + 1; t++) {
        task_t *task = dynarray_getelem(&tasks, t);
        if (task) {
            if (task->state == READY) {
                tid_ret = task->tid;
                dynarray_unref(&tasks, t);
                return tid_ret;
            }
        }
        dynarray_unref(&tasks, t);
    }

    return tid_ret; // Return -1 by default for idle
}

void yield() {
    if (scheduler_enabled) {
        lock(scheduler_lock);
        asm volatile("int $254");
    }
}

void force_unlocked_schedule() {
    if (scheduler_enabled) {
        asm volatile("int $254");
    }
}

void schedule_bsp(int_reg_t *r) {
    send_scheduler_ipis();

    if (spinlock_check_and_lock(&scheduler_lock.lock_dat)) {
        if (cpu_holding_sched_lock != -1 && cpu_holding_sched_lock != get_cpu_locals()->cpu_index) {
            lock(scheduler_lock);
        } else {
            return;
        }
    }
    schedule(r);
}

void schedule_ap(int_reg_t *r) {
    if (spinlock_check_and_lock(&scheduler_lock.lock_dat)) {
        if (cpu_holding_sched_lock != -1 && cpu_holding_sched_lock != get_cpu_locals()->cpu_index) {
            lock(scheduler_lock);
        } else {
            return;
        }
    }
    schedule(r);
}

void schedule(int_reg_t *r) {
    task_t *running_task = get_cpu_locals()->current_thread;
    if (running_task) {
        if (running_task->tid == get_cpu_locals()->idle_tid) {
            end_idle(); // End idling timer for this CPU
        }

        running_task->regs.rax = r->rax;
        running_task->regs.rbx = r->rbx;
        running_task->regs.rcx = r->rcx;
        running_task->regs.rdx = r->rdx;
        running_task->regs.rbp = r->rbp;
        running_task->regs.rdi = r->rdi;
        running_task->regs.rsi = r->rsi;
        running_task->regs.r8 = r->r8;
        running_task->regs.r9 = r->r9;
        running_task->regs.r10 = r->r10;
        running_task->regs.r11 = r->r11;
        running_task->regs.r12 = r->r12;
        running_task->regs.r13 = r->r13;
        running_task->regs.r14 = r->r14;
        running_task->regs.r15 = r->r15;

        running_task->regs.rflags = r->rflags;
        running_task->regs.rip = r->rip;
        running_task->regs.rsp = r->rsp;

        running_task->regs.cs = r->cs;
        running_task->regs.ss = r->ss;

        running_task->kernel_stack = get_cpu_locals()->thread_kernel_stack;
        running_task->user_stack = get_cpu_locals()->thread_user_stack;

        running_task->regs.cr3 = vmm_get_pml4t();

        running_task->tsc_stopped = read_tsc();
        running_task->tsc_total += running_task->tsc_stopped - running_task->tsc_started;

        if (running_task->running) {
            running_task->running = 0;
        }

        /* If we were previously running the task, then it is ready again since we are switching */
        if (running_task->state == RUNNING && running_task->tid != get_cpu_locals()->idle_tid) {
            running_task->state = READY;
            assert(running_task->state == READY);
        }

        /* Unref the running task, so we can swap it out */
        dynarray_unref(&tasks, running_task->tid);
    }

    // Run the next thread
    int64_t tid_run = pick_task();

    if (tid_run == -1) {
        /* Idle */
        get_cpu_locals()->current_thread = dynarray_getelem(&tasks, get_cpu_locals()->idle_tid);
        running_task = get_cpu_locals()->current_thread;
    } else {
        get_cpu_locals()->current_thread = dynarray_getelem(&tasks, tid_run);
        running_task = get_cpu_locals()->current_thread;
    }

    if (tid_run != -1) {
        assert(running_task->state == READY);
        assert(running_task->running == 0);
        running_task->running = 1;
        running_task->state = RUNNING;
        assert(running_task->state == RUNNING);
    }

    r->rax = running_task->regs.rax;
    r->rbx = running_task->regs.rbx;
    r->rcx = running_task->regs.rcx;
    r->rdx = running_task->regs.rdx;
    r->rbp = running_task->regs.rbp;
    r->rdi = running_task->regs.rdi;
    r->rsi = running_task->regs.rsi;
    r->r8 = running_task->regs.r8;
    r->r9 = running_task->regs.r9;
    r->r10 = running_task->regs.r10;
    r->r11 = running_task->regs.r11;
    r->r12 = running_task->regs.r12;
    r->r13 = running_task->regs.r13;
    r->r14 = running_task->regs.r14;
    r->r15 = running_task->regs.r15;

    r->rflags = running_task->regs.rflags;
    r->rip = running_task->regs.rip;
    r->rsp = running_task->regs.rsp;

    r->cs = running_task->regs.cs;
    r->ss = running_task->regs.ss;

    write_msr(0xC0000100, running_task->regs.fs); // Set FS.base

    get_thread_locals()->tid = running_task->tid;
    get_cpu_locals()->thread_kernel_stack = running_task->kernel_stack;
    get_cpu_locals()->thread_user_stack = running_task->user_stack;

    if (vmm_get_pml4t() != running_task->regs.cr3) {
        vmm_set_pml4t(running_task->regs.cr3);
    }

    running_task->tsc_started = read_tsc();

    if (tid_run == -1) {
        start_idle(); // Start idling timer
    }

    get_cpu_locals()->total_tsc = read_tsc();

    unlock(scheduler_lock);
}

int kill_task(int64_t tid) {
    int ret = 0;
    lock(scheduler_lock);
    cpu_holding_sched_lock = get_cpu_locals()->cpu_index;

    task_t *task = dynarray_getelem(&tasks, tid);
    if (task) {
        // If we are running this thread, unref it a first time because it is refed from running
        if (task->tid == get_cpu_locals()->current_thread->tid) {
            dynarray_unref(&tasks, tid);
            get_cpu_locals()->current_thread = (void *) 0;
        }
        dynarray_remove(&tasks, tid);
        dynarray_unref(&tasks, tid);
    } else {
        ret = 1;
    }
    cpu_holding_sched_lock = -1;
    force_unlocked_schedule();
    return ret; // make gcc happy
}

int kill_process(int64_t pid) {
    process_t *proc = dynarray_getelem(&processes, pid);
    if (proc) {
        for (int64_t t = 0; t < proc->threads.array_size; t++) {
            int64_t *tid = dynarray_getelem(&proc->threads, t);
            if (tid) {
                kill_task(*tid);
            }
        }

    }
    dynarray_remove(&processes, pid);
    dynarray_unref(&processes, pid);
    return 0;
}

process_t *reference_process(int64_t pid) {
    lock(scheduler_lock);
    cpu_holding_sched_lock = get_cpu_locals()->cpu_index;

    process_t *ret = (process_t *) dynarray_getelem(&processes, pid);

    cpu_holding_sched_lock = -1;
    unlock(scheduler_lock);
    return ret;
}

void deref_process(int64_t pid) {
    lock(scheduler_lock);
    cpu_holding_sched_lock = get_cpu_locals()->cpu_index;

    dynarray_unref(&processes, pid);

    cpu_holding_sched_lock = -1;
    unlock(scheduler_lock);
}

int map_user_memory(int pid, void *phys, void *virt, uint64_t size, uint16_t perms) {
    size = ((size + 0x1000 - 1) / 0x1000);
    process_t *process = reference_process(pid);
    if (!process) return -1;

    void *cr3 = (void *) process->cr3;
    int ret = vmm_map_pages(phys, virt, cr3, size, perms | VMM_PRESENT | VMM_USER);
    deref_process(pid);

    return ret;
}

int unmap_user_memory(int pid, void *virt, uint64_t size) {
    size = ((size + 0x1000 - 1) / 0x1000);
    process_t *process = reference_process(pid);
    if (!process) return -1;

    void *cr3 = (void *) process->cr3;
    int ret = vmm_unmap_pages(virt, cr3, size);
    deref_process(pid);

    return ret;
}

void *psuedo_mmap(void *base, uint64_t len) {
    len = (len + 0x1000 - 1) / 0x1000;
    process_t *process = reference_process(get_cpu_locals()->current_thread->parent_pid);
    if (!process) { get_thread_locals()->errno = -ESRCH; return (void *) 0; } // bruh

    void *phys = pmm_alloc(len * 0x1000);

    if (base) {
        uint64_t mapped = 0;
        int map_err = 0;
        sprintf("\n[MMAP] Mapping at base = %lx", base);
        for (uint64_t i = 0; i < len; i++) {
            int err = vmm_map_pages(phys, (void *) ((uint64_t) base + i * 0x1000), 
                (void *) process->cr3, 1, 
                VMM_WRITE | VMM_USER | VMM_PRESENT);
            
            if (err) {
                sprintf("\nVMM returned %d", err);
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
            get_thread_locals()->errno = -ENOMEM;
            return (void *) 0;
        } else {
            void *ret = (void *) base;
            return ret;
        }
    } else {
        lock(process->brk_lock);
        uint64_t mapped = 0;
        int map_err = 0;
        sprintf("\n[MMAP] Mapping at current_brk = %lx", process->current_brk);
        for (uint64_t i = 0; i < len; i++) {
            int err = vmm_map_pages(phys, (void *) ((uint64_t) process->current_brk + i * 0x1000), 
                (void *) process->cr3, 1, 
                VMM_WRITE | VMM_USER | VMM_PRESENT);
            
            if (err) {
                sprintf("\nVMM returned %d", err);
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
            get_thread_locals()->errno = -ENOMEM;

            unlock(process->brk_lock);
            sprintf("\nhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh");
            return (void *) 0;
        } else {
            void *ret = (void *) process->current_brk;
            process->current_brk += len * 0x1000;

            unlock(process->brk_lock);
            sprintf("\nhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh2");
            sprintf(" = %lx", ret);
            return ret;
        }
    }
}

int munmap(char *addr, uint64_t len) {
    len = (len + 0x1000 - 1) / 0x1000;
    process_t *process = reference_process(get_cpu_locals()->current_thread->parent_pid);
    if (!process) { get_thread_locals()->errno = -ESRCH; return -1; } // bruh

    if ((uint64_t) addr != ((uint64_t) addr & ~(0xfff))) {
        get_thread_locals()->errno = -EINVAL;
        return -1;
    }
    if ((uint64_t) addr > 0x7fffffffffff) {
        // Not user memory, stupid userspace
        get_thread_locals()->errno = -EINVAL;
        return -1;
    }

    for (uint64_t i = 0; i < len; i++) {
        vmm_unmap(addr, 1);
        addr += 0x1000;
    }
    return 0;
}

int fork(syscall_reg_t *r) {
    process_t *process = reference_process(get_cpu_locals()->current_thread->parent_pid); // Old process
    void *new_cr3 = vmm_fork((void *) process->cr3); // Fork address space
    int64_t new_pid = new_process(process->name, new_cr3); // Create the new process
    process_t *new_process = reference_process(new_pid); // Get the process struct

    /* Copy fds */
    clone_fds(process->pid, new_process->pid);

    lock(scheduler_lock);
    cpu_holding_sched_lock = get_cpu_locals()->cpu_index;

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
    task_t *old_thread = get_cpu_locals()->current_thread;

    task_t *thread = create_thread(old_thread->name, (void (*)()) old_thread->regs.rip, old_thread->regs.rsp, old_thread->ring);
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

    cpu_holding_sched_lock = -1;
    unlock(scheduler_lock);

    add_new_child_thread(thread, new_pid); // Add the thread

    /* Bye bye */
    deref_process(new_pid);
    deref_process(process->pid);
    return new_pid;
}