#include "scheduler.h"
#include "klibc/dynarray.h"
#include "klibc/vector.h"
#include "klibc/string.h"
#include "drivers/tty/tty.h"
#include "drivers/serial.h"
#include "io/msr.h"
#include "mm/vmm.h"

task_t *running_task;
dynarray_new(task_t, tasks);
uint8_t started = 0;
lock_t scheduler_lock = 0;

task_regs_t default_kernel_regs = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0x10,0x8,0,0x202,0};

void main_task() {
    kprintf("\nLoaded multitasking uwu");
    uint64_t count = 0;
    while (1) {
        sprintf("\nnob %lu", count++);
    }
}

void second_task() {
    kprintf("\nSecond task started");
    uint64_t count = 0;
    while (1) {
        sprintf("\nnob2 %lu", count++);
    }
}

void scheduler_init_bsp() {
    /* Start up scheduler for the BSP */
    sprintf("\n[SCHEDULER] Initializing tasking on the BSP");
    task_t starting_task;
    starting_task.regs = default_kernel_regs;
    starting_task.times_selected = 0;
    starting_task.state = 0;
    starting_task.regs.fs = (uint64_t) kcalloc(sizeof(thread_info_block_t));
    ((thread_info_block_t *) starting_task.regs.fs)->meta_pointer = starting_task.regs.fs;
    starting_task.regs.cr3 = vmm_get_pml4t();
    starting_task.regs.rip = (uint64_t) main_task;
    starting_task.regs.rsp = (uint64_t) kcalloc(TASK_STACK);
    sprintf("\n[SCHEDULER] Set Regs");

    write_msr(0xC0000100, starting_task.regs.fs);
    sprintf("\n[SCHEDULER] Set MSR");

    sprintf("\n[SCHEDULER] Added item %u", (uint32_t) dynarray_add(task_t, tasks, &starting_task));

    task_t starting_task2;
    starting_task2.regs = default_kernel_regs;
    starting_task2.times_selected = 0;
    starting_task2.state = 0;
    starting_task2.regs.fs = (uint64_t) kcalloc(sizeof(thread_info_block_t));
    ((thread_info_block_t *) starting_task2.regs.fs)->meta_pointer = starting_task2.regs.fs;
    starting_task2.regs.cr3 = vmm_get_pml4t();
    starting_task2.regs.rip = (uint64_t) second_task;
    starting_task2.regs.rsp = (uint64_t) kcalloc(TASK_STACK);
    sprintf("\n[SCHEDULER] Set Regs");

    write_msr(0xC0000100, starting_task2.regs.fs);
    sprintf("\n[SCHEDULER] Set MSR");

    sprintf("\n[SCHEDULER] Added item %u", (uint32_t) dynarray_add(task_t, tasks, &starting_task2));
    running_task = dynarray_getelem(task_t, tasks, 0);
    dynarray_unref(tasks, 0);
}

void schedule(int_reg_t *r) {
    sprintf("\n[SCHEDULER] Spinning scheduler lock ... ");
    spinlock_lock(&scheduler_lock);
    sprintf("Got lock.");
    uint64_t item_count = tasks_i;
    sprintf("\nItem count %lu", item_count);
    task_t *lowest_selected = (task_t *) 0;
    uint64_t lowest_selected_count = 0xFFFFFFFFFFFFFFFF;
    uint64_t lowest_selected_index = 0xff;
    for (uint64_t i = 0; i < item_count; i++) {
        task_t *task = dynarray_getelem(task_t, tasks, i);
        if (task) {
            sprintf("\nCount: %lu", task->times_selected);
            sprintf("\nLowest: %lu", lowest_selected_count);
            if (task->times_selected < lowest_selected_count) {
                lowest_selected = task;
                lowest_selected_count = task->times_selected;
                lowest_selected_index = i;
            }
            dynarray_unref(tasks, i);
        }
    }

    if (!lowest_selected) {
        lowest_selected = dynarray_getelem(task_t, tasks, 0);
        dynarray_unref(tasks, 0);
    }

    lowest_selected->times_selected++;

    sprintf("\nRIP of new: %lx", lowest_selected->regs.rip);
    sprintf("\nRIP of old %lx", r->rip);
    sprintf("\nSelected: %lu", lowest_selected_index);

    /* Save old registers */
    if (started) {
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

        running_task->regs.rsp = r->rsp;
        running_task->regs.rip = r->rip;

        running_task->regs.cs = r->cs;
        running_task->regs.ss = r->ss;
        uint64_t fs;
        asm volatile("movq %%fs:(0), %0;" : "=r"(fs));
        running_task->regs.fs = fs;
        running_task->regs.cr3 = vmm_get_pml4t();
    }

    started = 1;
    running_task = lowest_selected;

    /* Set new registers */
    r->rax = lowest_selected->regs.rax;
    r->rbx = lowest_selected->regs.rbx;
    r->rcx = lowest_selected->regs.rcx;
    r->rdx = lowest_selected->regs.rdx;
    r->rbp = lowest_selected->regs.rbp;
    r->rdi = lowest_selected->regs.rdi;
    r->rsi = lowest_selected->regs.rsi;
    r->r8 = lowest_selected->regs.r8;
    r->r9 = lowest_selected->regs.r9;
    r->r10 = lowest_selected->regs.r10;
    r->r11 = lowest_selected->regs.r11;
    r->r12 = lowest_selected->regs.r12;
    r->r13 = lowest_selected->regs.r13;
    r->r14 = lowest_selected->regs.r14;
    r->r15 = lowest_selected->regs.r15;

    r->rsp = lowest_selected->regs.rsp;
    r->rip = lowest_selected->regs.rip;

    r->cs = lowest_selected->regs.cs;
    r->ss = lowest_selected->regs.ss;
    write_msr(0xC0000100, lowest_selected->regs.fs); // Write to FS.Base
    if (vmm_get_pml4t() != lowest_selected->regs.cr3) {
        vmm_set_pml4t(lowest_selected->regs.cr3);
    }

    spinlock_unlock(&scheduler_lock);
}

