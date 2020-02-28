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

void send_scheduler_ipis() {
    madt_ent0_t **cpus = (madt_ent0_t **) vector_items(&cpu_vector);
    for (uint64_t i = 0; i < cpu_vector.items_count; i++) {
        if ((cpus[i]->cpu_flags & 1 || cpus[i]->cpu_flags & 2) && cpus[i]->apic_id != get_lapic_id()) {
            send_ipi(cpus[i]->apic_id, (1 << 14) | 253); // Send interrupt 253
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

void main_task() {
    sprintf("\nLoaded multitasking uwu");
    //uint64_t count = 0;
    while (1) {
        kprintf_at(0, 0, "\nCounter 1: %lu", global_ticks);
    }
}

void second_task() {
    sprintf("\nSecond task scheduler_started");
    uint64_t count = 0;
    while (1) {
        kprintf("\nCounter 2: %lu", count++);
    }
}

void scheduler_init_bsp() {
    tasks.array_size = 0;
    tasks.base = 0;
    processes.array_size = 0;
    processes.base = 0;

    new_process(main_task, (void *) vmm_get_pml4t() + VM_OFFSET, "Main task");
    new_process(second_task, (void *) vmm_get_pml4t() + VM_OFFSET, "Second task");

    /* Setup the idle task */

    task_t *idle_no_array = new_task(_idle, (void *) vmm_get_pml4t() + VM_OFFSET, "_idle");
    task_t *idle_task = dynarray_getelem(&tasks, idle_no_array->tid);
    idle_task->state = BLOCKED;
    get_cpu_locals()->idle_tid = idle_task->tid;
    dynarray_unref(&tasks, get_cpu_locals()->idle_tid);

    for (int64_t p = 0; p < processes.array_size; p++) {
        process_t *proc = dynarray_getelem(&processes, p);
        if (proc) {
            sprintf("\nProc name: %s", proc->name);
            sprintf("\nPID: %ld", proc->pid);
            for (int64_t t = 0; t < proc->threads.array_size; t++) {
                task_t *task = dynarray_getelem(&proc->threads, t);
                if (task) {
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
    task_t *idle_no_array = new_task(_idle, (void *) vmm_get_pml4t() + VM_OFFSET, "_idle");
    task_t *idle_task = dynarray_getelem(&tasks, idle_no_array->tid);
    idle_task->state = BLOCKED;
    get_cpu_locals()->idle_tid = idle_task->tid;
    dynarray_unref(&tasks, get_cpu_locals()->idle_tid);
}

task_t *new_task(void (*main)(), void *parent_addr_space_cr3, char *name) {
    lock(&scheduler_lock);

    task_t *task = kcalloc(sizeof(task_t));

    /* Setup new task */
    task->regs = default_kernel_regs;
    task->regs.rip = (uint64_t) main;

    /* Create new address space */
    uint64_t new_addr_space = (uint64_t) kcalloc(0x1000);
    new_addr_space -= NORMAL_VMA_OFFSET;
    task->regs.cr3 = new_addr_space;
    if (parent_addr_space_cr3) {
        /* Copy old address space */
        memcpy((uint8_t *) parent_addr_space_cr3, (uint8_t *) new_addr_space + NORMAL_VMA_OFFSET, 0x1000);
    } else {
        /* Copy the higher half mappings */
        memcpy((uint8_t *) vmm_get_pml4t() + 0x800 + NORMAL_VMA_OFFSET, (uint8_t *) task->regs.cr3 + 0x800 + NORMAL_VMA_OFFSET,
            0x800);
    }

    /* Create new thread info block */
    thread_info_block_t *info = kmalloc(sizeof(thread_info_block_t));
    info->meta_pointer = (uint64_t) info;
    task->regs.fs = (uint64_t) info;
    
    /* Create new task stack */
    task->regs.rsp = (uint64_t) kcalloc(TASK_STACK_SIZE) + TASK_STACK_SIZE; // Go to the end of the stack

    /* Set the name */
    strcpy(name, task->name);

    /* Initialize the other fields */
    task->state = READY;
    int tid = dynarray_add(&tasks, task, sizeof(task_t));
    task_t *task_arr = dynarray_getelem(&tasks, tid);
    task_arr->tid = tid;
    task->tid = tid;
    /* Default parent pid is -1 */
    task->parent_pid = -1;
    task_arr->parent_pid = -1;
    dynarray_unref(&tasks, tid);

    unlock(&scheduler_lock);
    return task;
}

int64_t new_child_task(void (*main)(), void *parent_addr_space_cr3, char *name, int64_t parent_pid) {
    task_t *created_task = new_task(main, parent_addr_space_cr3, name);
    
    task_t *array_task = dynarray_getelem(&tasks, created_task->tid);
    process_t *array_parent = dynarray_getelem(&processes, parent_pid);
    /* Set the parent PIDs */
    created_task->parent_pid = parent_pid;
    array_task->parent_pid = parent_pid;

    dynarray_add(&array_parent->threads, created_task, sizeof(task_t));

    dynarray_unref(&tasks, parent_pid);
    dynarray_unref(&tasks, created_task->tid);

    return created_task->tid;
}

int new_process(void (*main)(), void *parent_addr_space_cr3, char *name) {
    process_t *process = kcalloc(sizeof(process_t));
    process->threads.base = 0;
    process->threads.array_size = 0;

    task_t *base_task = new_task(main, parent_addr_space_cr3, name);
    
    /* Lock the scheduler here so that new_task doesnt try to lock when we have the lock */
    lock(&scheduler_lock);

    /* Set name and PID */
    int pid = dynarray_add(&processes, process, sizeof(process_t));
    process_t *elem = dynarray_getelem(&processes, pid);
    elem->pid = pid;
    strcpy(name, elem->name);

    /* Set the base_task parent pid to our new pid */
    base_task->parent_pid = pid;

    /* Add the task and process to the threads dynarray for the process */
    dynarray_add(&elem->threads, base_task, sizeof(task_t));

    /* Set the parent PID in the global threads list */
    task_t *task_arr_elem = dynarray_getelem(&tasks, base_task->tid);
    task_arr_elem->parent_pid = pid;

    dynarray_unref(&tasks, base_task->tid);

    dynarray_unref(&processes, pid);

    /* Cleanup, since items are in the dynarray */
    kfree(base_task);
    kfree(process);

    unlock(&scheduler_lock);
    return pid;
}

int64_t pick_task() {
    int64_t tid_ret = -1;

    if (!(get_cpu_locals()->current_thread)) {
        return -1;
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
    //sprintf("\nBSP scheduling");
    send_scheduler_ipis();
    schedule(r);
}

void schedule_ap(int_reg_t *r) {
    sprintf("\nScheduling in the AP");
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

        running_task->regs.cr3 = vmm_get_pml4t();

        /* If we were previously running the task, then it is ready again since we are switching */
        if (running_task->state == RUNNING) {
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

    running_task->state = RUNNING;

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

    if (vmm_get_pml4t() != running_task->regs.cr3) {
        vmm_set_pml4t(running_task->regs.cr3);
    }

    if (tid_run == -1) {
        start_idle();
    }

    unlock(&scheduler_lock);
}