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

worker_stack_t worker_stack;

dynarray_t tasks;
dynarray_t processes;

uint8_t scheduler_enabled = 0;
lock_t scheduler_lock = {0, 0, 0};

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

/* Launch a high priority kernel worker */
void launch_kernel_worker(void (*worker)()) {
    lock(scheduler_lock);
    worker_stack_t *cur = &worker_stack;
    while (cur->next) {
        cur = cur->next;
    }

    /* Allocate new on the "stack" */
    cur->next = kcalloc(sizeof(worker_stack_t));
    cur = cur->next;
    
    cur->thread.regs = default_kernel_regs;
    cur->thread.kernel_stack = (uint64_t) kcalloc(0x1000) + 0x1000;
    cur->thread.regs.rsp = (uint64_t) kcalloc(0x1000) + 0x1000;
    cur->thread.regs.cr3 = base_kernel_cr3;
    cur->thread.regs.rip = (uint64_t) worker;

    cur->thread.ring = 0;
    cur->thread.state = READY;

    cur->thread.regs.fs = (uint64_t) kcalloc(sizeof(thread_info_block_t));
    ((thread_info_block_t *) cur->thread.regs.fs)->meta_pointer = cur->thread.regs.fs;
    unlock(scheduler_lock);
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
    kfree(new_task);
    return new_tid;
}

/* Add the thread to the dynarray */
int64_t start_thread(task_t *thread) {
    int64_t tid = dynarray_add(&tasks, thread, sizeof(task_t));
    kfree(thread);
    thread = dynarray_getelem(&tasks, tid);
    thread->tid = tid;
    dynarray_unref(&tasks, tid);

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
    ((thread_info_block_t *) new_task->regs.fs)->meta_pointer = new_task->regs.fs;
    new_task->state = READY;
    strcpy(name, new_task->name);

    return new_task;
}

/* Add a new thread to the dynarray and as a child of a process */
int64_t add_new_child_thread(task_t *task, int64_t pid) {
    interrupt_state_t state = interrupt_lock();
    lock(scheduler_lock);

    /* Find the parent process */
    process_t *new_parent = dynarray_getelem(&processes, pid);
    if (!new_parent) { sprintf("\n[Scheduler] Couldn't find parent"); return -1; }

    /* Store the new task and save its TID */
    int64_t new_tid = dynarray_add(&tasks, task, sizeof(task_t));
    task_t *task_item = dynarray_getelem(&tasks, new_tid);
    task_item->tid = new_tid;
    task_item->regs.cr3 = new_parent->cr3; // Inherit parent's cr3
    task_item->parent_pid = pid;
    dynarray_unref(&tasks, new_tid);
    task->tid = new_tid;
    task->parent_pid = pid;

    /* Add the TID to it's parent's threads list */
    dynarray_add(&new_parent->threads, &new_tid, sizeof(int64_t));

    unlock(scheduler_lock);
    interrupt_unlock(state);
    return new_tid;
}

/* Allocate new data for a process, then return the new PID */
int64_t new_process(char *name, void *new_cr3) {
    interrupt_state_t state = interrupt_lock();
    lock(scheduler_lock);

    /* Allocate new process */
    process_t *new_process = kcalloc(sizeof(process_t));
    /* New cr3 */
    new_process->cr3 = (uint64_t) new_cr3;
    /* Default UID and GID */
    new_process->uid = 0;
    new_process->gid = 0;
    new_process->fd_table = kcalloc(10 * sizeof(fd_entry_t));
    new_process->fd_table_size = 10;
    new_process->allocation_table = kcalloc(sizeof(rangemap_t));
    strcpy(name, new_process->name);

    /* Add element to the dynarray and save the PID */
    int64_t pid = dynarray_add(&processes, (void *) new_process, sizeof(process_t));
    process_t *process_item = dynarray_getelem(&processes, pid);
    process_item->pid = pid;
    dynarray_unref(&processes, pid);

    unlock(scheduler_lock);
    interrupt_unlock(state);
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

    lock(scheduler_lock);
    schedule(r);
}

void schedule_ap(int_reg_t *r) {
    lock(scheduler_lock);
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
        sprintf("\ntid_run: %ld, task: %s", tid_run, running_task->name);
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
    interrupt_state_t state = interrupt_lock();
    lock(scheduler_lock);
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
    force_unlocked_schedule();
    interrupt_unlock(state);
    return ret; // make gcc happy
}

int kill_process(int64_t pid) {
    interrupt_state_t state = interrupt_lock();
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
    interrupt_unlock(state);
    return 0;
}

process_t *reference_process(int64_t pid) {
    process_t *ret = (process_t *) dynarray_getelem(&processes, pid);
    return ret;
}

void deref_process(int64_t pid) {
    dynarray_unref(&processes, pid);
}

void *allocate_process_mem(int pid, uint64_t size) {
    size = ((size + 0x1000 - 1) / 0x1000) * 0x1000;
    process_t *process = reference_process(pid);
    if (!process) return (void *) 0;

    uint64_t free = rangemap_find_free_area(process->allocation_table, size);
    if (free + size > 0x7fffffffffff) return (void *) 0; // Higher than usable memory

    rangemap_add_range(process->allocation_table, free, free + size);
    deref_process(pid);
    return (void *) free;
}

int free_process_mem(int pid, uint64_t address) {
    process_t *process = reference_process(pid);
    if (!process) return -1;

    if (rangemap_entry_present(process->allocation_table, address)) return -2;

    rangemap_mark_free(process->allocation_table, address);
    deref_process(pid);

    return 0;
}

int mark_process_mem_used(int pid, uint64_t addr, uint64_t size) {
    size = ((size + 0x1000 - 1) / 0x1000) * 0x1000;
    process_t *process = reference_process(pid);
    if (!process) return -1;

    if (rangemap_entry_present(process->allocation_table, addr)) return -2;
    rangemap_add_range(process->allocation_table, addr, addr + size);
    deref_process(pid);

    return 0;
}

int map_user_memory(int pid, void *phys, void *virt, uint64_t size, uint16_t perms) {
    size = ((size + 0x1000 - 1) / 0x1000);
    process_t *process = reference_process(pid);
    if (!process) return -1;

    void *cr3 = (void *) process->cr3;
    return vmm_map_pages(phys, virt, cr3, size, perms | VMM_PRESENT | VMM_USER);
}

int unmap_user_memory(int pid, void *virt, uint64_t size) {
    size = ((size + 0x1000 - 1) / 0x1000);
    process_t *process = reference_process(pid);
    if (!process) return -1;

    void *cr3 = (void *) process->cr3;
    return vmm_unmap_pages(virt, cr3, size);
}


void *mmap(void *addr_hint, uint64_t size, int prot, int flags, int fd, uint64_t offset) {
    int pid = get_cpu_locals()->current_thread->parent_pid;
    void *memory = allocate_process_mem(pid, size);
    void *phys_mem = pmm_alloc(size);

    if (!memory) {
        get_thread_locals()->errno = -ENOMEM;
        pmm_unalloc(phys_mem, size);
        return (void *) 0;
    }

    int not_mapped = map_user_memory(pid, phys_mem, memory, size, VMM_WRITE);
    if (not_mapped != 0) {
        panic("Failed memory mapping!");

        /* We won't get control back, but meh */
        pmm_unalloc(phys_mem, size);
        return (void *) 0;
    }

    sprintf("\nAllocated %lu bytes for process %d. Phys: %lx, Virt: %lx", size, pid, phys_mem, memory);
    return memory;

    (void) addr_hint;
    (void) prot;
    (void) flags;
    (void) fd;
    (void) offset;
}