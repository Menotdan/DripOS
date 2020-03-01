#include "scheduler.h"
#include "klibc/dynarray.h"
#include "klibc/stdlib.h"
#include "klibc/string.h"
#include "drivers/tty/tty.h"
#include "drivers/serial.h"
#include "io/msr.h"
#include "mm/vmm.h"
#include "sys/smp.h"
#include "sys/apic.h"

#include "drivers/pit.h"

dynarray_t tasks;
dynarray_t processes;
uint8_t scheduler_enabled = 0;
lock_t scheduler_lock = 0;

task_regs_t default_kernel_regs = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0x10,0x8,0,0x202,0};
task_regs_t default_user_regs = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0x23,0x1B,0,0x202,0};

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

void user_task() {
    while (1) {
        asm volatile("nop");
    }
}

void main_task() {
    while (1) {
        //kprintf_at(0, 0, "TSC Idle: %lu TSC Running: %lu On CPU %u", get_cpu_locals()->idle_tsc_count, (get_cpu_locals()->total_tsc - get_cpu_locals()->idle_tsc_count), (uint32_t) get_cpu_locals()->cpu_index);
    }
}

void second_task() {
    //uint64_t count = 0;
    while (1) {
        //kprintf_at(0, 1, "Counter 2: %lu on core %u", count++, (uint32_t) get_cpu_locals()->cpu_index);
    }
}

void third_task() {
    while (1) {
        //kprintf_at(0, 2, "TSC Idle: %lu TSC Running: %lu On CPU %u", get_cpu_locals()->idle_tsc_count, (get_cpu_locals()->total_tsc - get_cpu_locals()->idle_tsc_count), (uint32_t) get_cpu_locals()->cpu_index);
    }
}

void scheduler_init_bsp() {
    tasks.array_size = 0;
    tasks.base = 0;
    processes.array_size = 0;
    processes.base = 0;

    new_process("Idle tasks"); // Always PID 0
    sprintf("\nCreated idle process");

    new_kernel_process("First task", main_task);
    new_kernel_process("Second task", second_task);
    new_kernel_process("Third task", third_task);

    /* Setup the idle task */

    uint64_t idle_rsp = (uint64_t) kcalloc(0x1000) + 0x1000;
    int64_t idle_tid = new_thread("idle", _idle, (void *) vmm_get_pml4t(), idle_rsp, 0, 0);
    task_t *idle_task = dynarray_getelem(&tasks, idle_tid);

    idle_task->state = BLOCKED;

    dynarray_unref(&tasks, idle_tid);

    get_cpu_locals()->idle_tid = idle_tid;

    sprintf("\nIdle task: %ld", get_cpu_locals()->idle_tid);

    for (int64_t p = 0; p < processes.array_size; p++) {
        process_t *proc = dynarray_getelem(&processes, p);
        if (proc) {
            sprintf("\nProc name: %s", proc->name);
            sprintf("\nPID: %ld", proc->pid);
            for (int64_t t = 0; t < proc->threads.array_size; t++) {
                int64_t *tid = dynarray_getelem(&proc->threads, t);
                if (tid) {
                    task_t *task = dynarray_getelem(&tasks, *tid);
                    sprintf("\n  Task for %ld", proc->pid);
                    sprintf("\n    Task name: %s", task->name);
                    sprintf("\n    TID: %ld", task->tid);
                    sprintf("\n    Parent pid: %ld", task->parent_pid);
                }
                dynarray_unref(&proc->threads, t);
            }
        }
        dynarray_unref(&processes, p);
    }
}

void scheduler_init_ap() {
    uint64_t idle_rsp = (uint64_t) kcalloc(0x1000) + 0x1000;
    int64_t idle_tid = new_thread("idle", _idle, (void *) vmm_get_pml4t(), idle_rsp, 0, 0);
    task_t *idle_task = dynarray_getelem(&tasks, idle_tid);

    idle_task->state = BLOCKED;

    dynarray_unref(&tasks, idle_tid);

    get_cpu_locals()->idle_tid = idle_tid;
    sprintf("\nIdle task: %ld", get_cpu_locals()->idle_tid);
}

int64_t new_thread(char *name, void (*main)(), void *new_cr3, uint64_t rsp, int64_t pid, uint8_t ring) {
    task_t *new_task = create_thread(name, main, new_cr3, rsp, ring);
    int64_t new_tid = add_new_child_thread(new_task, pid);
    kfree(new_task);
    return new_tid;
}

task_t *create_thread(char *name, void (*main)(), void *new_cr3, uint64_t rsp, uint8_t ring) {
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
    new_task->regs.cr3 = (uint64_t) new_cr3;
    new_task->regs.fs = (uint64_t) kcalloc(sizeof(thread_info_block_t));
    ((thread_info_block_t *) new_task->regs.fs)->meta_pointer = new_task->regs.fs;
    strcpy(name, new_task->name);

    return new_task;
}

/* Add a new thread to the dynarray */
int64_t add_new_thread(task_t *task) {
    lock(&scheduler_lock);

    int64_t new_tid = dynarray_add(&tasks, task, sizeof(task_t));
    task_t *task_item = dynarray_getelem(&tasks, new_tid);
    task_item->tid = new_tid;
    task->tid = new_tid;
    dynarray_unref(&tasks, new_tid);

    unlock(&scheduler_lock);
    return new_tid;
}

/* Add a new thread to the dynarray and as a child of a process */
int64_t add_new_child_thread(task_t *task, int64_t pid) {
    lock(&scheduler_lock);

    /* Find the parent process */
    process_t *new_parent = dynarray_getelem(&processes, pid);
    if (!new_parent) { sprintf("\n[Scheduler] Couldn't find parent"); return -1; }

    /* Store the new task and save its TID */
    int64_t new_tid = dynarray_add(&tasks, task, sizeof(task_t));
    task_t *task_item = dynarray_getelem(&tasks, new_tid);
    task_item->tid = new_tid;
    task_item->parent_pid = pid;
    dynarray_unref(&tasks, new_tid);
    task->tid = new_tid;
    task->parent_pid = pid;

    /* Add the TID to it's parent's threads list */
    dynarray_add(&new_parent->threads, &new_tid, sizeof(int64_t));

    unlock(&scheduler_lock);
    return new_tid;
}

int64_t new_process(char *name) {
    lock(&scheduler_lock);

    /* Allocate new process */
    process_t *new_process = kcalloc(sizeof(process_t));
    /* Default UID and GID */
    new_process->uid = 0;
    new_process->gid = 0;
    strcpy(name, new_process->name);

    /* Add element to the dynarray and save the PID */
    int64_t pid = dynarray_add(&processes, (void *) new_process, sizeof(process_t));
    process_t *process_item = dynarray_getelem(&processes, pid);
    process_item->pid = pid;
    dynarray_unref(&processes, pid);

    unlock(&scheduler_lock);
    /* Free the old data since it's in the dynarray */
    kfree(new_process);
    return pid;
}

void new_kernel_process(char *name, void (*main)()) {
    int64_t task_parent_pid = new_process(name);
    uint64_t new_rsp = (uint64_t) kcalloc(TASK_STACK_SIZE) + TASK_STACK_SIZE;
    new_thread(name, main, (void *) vmm_get_pml4t(), new_rsp, task_parent_pid, 0);
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

void schedule_bsp(int_reg_t *r) {
    send_scheduler_ipis();
    schedule(r);
}

void schedule_ap(int_reg_t *r) {
    schedule(r);
}

void schedule(int_reg_t *r) {
    lock(&scheduler_lock);

    task_t *running_task = get_cpu_locals()->current_thread;

    if (running_task) {
        if (running_task->tid == get_cpu_locals()->idle_tid) {
            end_idle(); // End idling for this CPU
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

        /* If we were previously running the task, then it is ready again since we are switching */
        if (running_task->state == RUNNING && running_task->tid != get_cpu_locals()->idle_tid) {
            running_task->state = READY;
        }

        /* Unref the running task, so we can swap it out */
        dynarray_unref(&tasks, running_task->tid);
    }

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
        running_task->state = RUNNING;
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

    get_cpu_locals()->thread_kernel_stack = running_task->kernel_stack;
    get_cpu_locals()->thread_user_stack = running_task->user_stack;

    if (vmm_get_pml4t() != running_task->regs.cr3) {
        vmm_set_pml4t(running_task->regs.cr3);
    }

    if (tid_run == -1) {
        start_idle();
    }

    get_cpu_locals()->total_tsc = read_tsc();

    unlock(&scheduler_lock);
}