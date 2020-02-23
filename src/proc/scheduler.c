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

void main_task() {
    kprintf("\nLoaded multitasking uwu");
    uint64_t count = 0;
    while (1) {
        kprintf("\nnob %lu", count++);
    }
}

void second_task() {
    kprintf("\nSecond task scheduler_started");
    uint64_t count = 0;
    while (1) {
        kprintf("\nnob2 %lu", count++);
    }
}

void scheduler_init_bsp() {
    tasks.array_size = 0;
    tasks.base = 0;
    processes.array_size = 0;
    processes.base = 0;

    new_process(main_task, (void *) 0, "Main task");
    new_process(second_task, (void *) 0, "Second task");

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
}

task_t *new_task(void (*main)(), void *parent_addr_space_cr3, char *name) {
    task_t *task = kcalloc(sizeof(task_t));

    /* Setup new task */
    task->regs = default_kernel_regs;
    task->regs.rip = (uint64_t) main;
    /* Create new address space */
    uint64_t new_addr_space = (uint64_t) kcalloc(0x1000);
    task->regs.cr3 = new_addr_space;
    if (parent_addr_space_cr3) {
        /* Copy old address space */
        memcpy((uint8_t *) parent_addr_space_cr3, (uint8_t *) new_addr_space, 0x1000);
    } else {
        /* Copy the higher half mappings */
        memcpy((uint8_t *) vmm_get_pml4t() + 0x800 + 0xFFFF800000000000, (uint8_t *) task->regs.cr3 + 0x800, 0x800);
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

    return task;
}

int new_process(void (*main)(), void *parent_addr_space_cr3, char *name) {
    process_t *process = kcalloc(sizeof(process_t));
    process->threads.base = 0;
    process->threads.array_size = 0;

    task_t *base_task = new_task(main, parent_addr_space_cr3, name);
    
    dynarray_add(&process->threads, base_task, sizeof(task_t));
    int pid = dynarray_add(&processes, process, sizeof(process_t));
    process_t *elem = dynarray_getelem(&processes, pid);
    elem->pid = pid;
    strcpy(name, elem->name);
    dynarray_unref(&processes, pid);

    /* Cleanup, since items are in the dynarray */
    kfree(base_task);
    kfree(process);

    return pid;
}