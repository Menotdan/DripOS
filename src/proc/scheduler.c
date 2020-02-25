#include "scheduler.h"
#include "klibc/dynarray.h"
#include "klibc/stdlib.h"
#include "klibc/string.h"
#include "drivers/tty/tty.h"
#include "drivers/serial.h"
#include "io/msr.h"
#include "mm/vmm.h"

task_t *running_task;
dynarray_t tasks;
dynarray_t processes;
uint8_t scheduler_started = 0;
lock_t scheduler_lock = 0;

task_regs_t default_kernel_regs = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0x10,0x8,0,0x202,0};

void _idle() {
    while (1) {
        asm volatile("hlt");
    }
}

void main_task() {
    sprintf("\nLoaded multitasking uwu");
    uint64_t count = 0;
    while (1) {
        tty_seek(0, 0, &base_tty);
        kprintf("\nnob %lu", count++);
        //yield();
    }
}

void second_task() {
    sprintf("\nSecond task scheduler_started");
    uint64_t count = 0;
    while (1) {
        tty_seek(0, 1, &base_tty);
        kprintf("\nnob2 %lu", count++);
        //yield();
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
    new_task(_idle, (void *) vmm_get_pml4t() + VM_OFFSET, "_idle");
    task_t *idle_task = dynarray_getelem(&tasks, 2);
    idle_task->state = BLOCKED; // Block the idle task, so it doesnt run
    dynarray_unref(&tasks, 2);

    for (int64_t p = 0; p < processes.array_size; p++) {
        process_t *proc = dynarray_getelem(&processes, p);
        if (proc) {
            sprintf("\nProc name: %s", proc->name);
            sprintf("\nPID: %ld", proc->pid);
        }
        dynarray_unref(&processes, p);
    }

    for (int64_t t = 0; t < tasks.array_size; t++) {
        task_t *task = dynarray_getelem(&tasks, t);
        if (task) {
            sprintf("\nTask name: %s", task->name);
            sprintf("\nTID: %ld", task->tid);
        }
        dynarray_unref(&tasks, t);
    }

    running_task = (task_t *) dynarray_getelem(&tasks, 0);
}

task_t *new_task(void (*main)(), void *parent_addr_space_cr3, char *name) {
    lock(&scheduler_lock);

    task_t *task = kcalloc(sizeof(task_t));

    /* Setup new task */
    task->regs = default_kernel_regs;
    task->regs.rip = (uint64_t) main;
    /* Create new address space */
    uint64_t new_addr_space = (uint64_t) kcalloc(0x1000);
    new_addr_space -= 0xFFFF800000000000;
    task->regs.cr3 = new_addr_space;
    if (parent_addr_space_cr3) {
        /* Copy old address space */
        memcpy((uint8_t *) parent_addr_space_cr3, (uint8_t *) new_addr_space + 0xFFFF800000000000, 0x1000);
    } else {
        /* Copy the higher half mappings */
        memcpy((uint8_t *) vmm_get_pml4t() + 0x800 + 0xFFFF800000000000, (uint8_t *) task->regs.cr3 + 0x800 + 0xFFFF800000000000,
            0x800);
    }

    /* Create new thread info block */
    thread_info_block_t *info = kmalloc(sizeof(thread_info_block_t));
    info->meta_pointer = (uint64_t) info;
    task->regs.fs = (uint64_t) info;
    
    /* Create new task stack */
    task->regs.rsp = (uint64_t) kcalloc(TASK_STACK_SIZE) + TASK_STACK_SIZE;

    /* Set the name */
    strcpy(name, task->name);

    /* Initialize the other fields */
    task->state = READY;
    int tid = dynarray_add(&tasks, (void *) task, sizeof(task_t));
    task_t *task_arr = dynarray_getelem(&tasks, tid);
    task_arr->tid = tid;
    task->tid = tid;
    dynarray_unref(&tasks, tid);

    unlock(&scheduler_lock);
    return task;
}

int new_process(void (*main)(), void *parent_addr_space_cr3, char *name) {
    process_t *process = kcalloc(sizeof(process_t));
    process->threads.base = 0;
    process->threads.array_size = 0;

    task_t *base_task = new_task(main, parent_addr_space_cr3, name);
    
    /* Lock the scheduler here so that new_task doesnt try to lock when we have the lock */
    lock(&scheduler_lock);

    /* Add the task and process to the dynarray */
    dynarray_add(&process->threads, base_task, sizeof(task_t));

    int pid = dynarray_add(&processes, process, sizeof(process_t));
    process_t *elem = dynarray_getelem(&processes, pid);
    elem->pid = pid;
    strcpy(name, elem->name);
    dynarray_unref(&processes, pid);

    /* Cleanup, since items are in the dynarray */
    kfree(base_task);
    kfree(process);

    unlock(&scheduler_lock);
    return pid;
}

int64_t pick_task() {
    int64_t tid_ret = -1;
    /* Prioritze tasks right after the current task */
    for (int64_t t = running_task->tid + 1; t < tasks.array_size; t++) {
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
    for (int64_t t = 0; t < running_task->tid + 1; t++) {
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

void schedule(int_reg_t *r) {
    lock(&scheduler_lock);

    if (scheduler_started) {
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
    }
    scheduler_started = 1;

    int64_t tid_run = pick_task();

    /* Unref the running task, so we can change it */
    dynarray_unref(&tasks, running_task->tid);
    if (tid_run == -1) {
        /* Idle */
        running_task = dynarray_getelem(&tasks, 2);
    } else {
        running_task = dynarray_getelem(&tasks, tid_run);
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

    //sprintf("\nRAX: %lx RBX: %lx RCX: %lx \nRDX: %lx RBP: %lx RDI: %lx \nRSI: %lx R08: %lx R09: %lx \nR10: %lx R11: %lx R12: %lx \nR13: %lx R14: %lx R15: %lx \nRSP: %lx ERR: %lx INT: %lx \nRIP: %lx CS: %lx SS: %lx", r->rax, r->rbx, r->rcx, r->rdx, r->rbp, r->rdi, r->rsi, r->r8, r->r9, r->r10, r->r11, r->r12, r->r13, r->r14, r->r15, r->rsp, r->int_err, r->int_num, r->rip, r->cs, r->ss);

    if (vmm_get_pml4t() != running_task->regs.cr3) {
        vmm_set_pml4t(running_task->regs.cr3);
    }

    unlock(&scheduler_lock);
}