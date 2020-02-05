#include "task.h"

Task *running_task;
Task main_task;
Task kickstart;
static Task temp;
uint32_t old_stack_ptr;
uint32_t pid_max = 0;
uint32_t global_esp = 0;
uint32_t global_esp_old = 0;
uint32_t task_size = sizeof(Task);
Task *global_old_task;
Task *dead_task_queue = (Task *)0;
Registers *regs;
Task *focus_tasks;
extern uint32_t suicide_stack;

Task *get_task_from_pid(uint32_t pid) {
    Task *temp_task = (&main_task)->next;
    while (temp_task->pid != 0) {
        if (temp_task->pid == pid) {
            break;
        }
        temp_task = temp_task->next;
    }
    if (pid != 0 && temp_task->pid == 0) {
        return (Task *)0;
    }
    return temp_task;
}

static void otherMain() {
    loaded = 1;
    sprintf("\nIn main");
    while (1) {
        asm volatile("hlt");
    }
}

static void task_cleaner() {
    Task *next;
    while (1) {
        sleep(2000);
        while (dead_task_queue != 0) {
            next = dead_task_queue->next_dead;
            kill_task(dead_task_queue->pid);
            dead_task_queue = next;
        }
    }
}

void set_focused_task(Task *new_focus) {
    focus_tasks = new_focus;
}

Task *get_focused_task() {
    return focus_tasks;
}

void init_tasking() {
    // Get EFLAGS and CR3
    asm volatile("movq %%cr3, %%rax; movq %%rax, %0;"
                 : "=m"(temp.regs.cr3)::"%rax"); // No paging yet
    asm volatile("pushfq; movq (%%rsp), %%rax; movq %%rax, %0; popfq;"
                 : "=m"(temp.regs.rflags)::"%rax");

    create_task(&main_task, otherMain, "Idle task");
    create_task(&kickstart, 0, "no");
    pid_max = 1;
    main_task.next = &main_task;
    kickstart.next = &main_task;
    main_task.cursor_pos = CURSOR_MAX;
    Task *cleaner = kmalloc(sizeof(Task));
    create_task(cleaner, task_cleaner, "Task cleaner");
    cleaner->cursor_pos = CURSOR_MAX;
    sprintf("\nCleaner PID: %u", cleaner->pid);
    // while (1) {
    //     asm volatile("hlt");
    // }

    init_terminal();
    running_task = &kickstart;
    otherMain();
}

uint32_t create_task(Task *task, void (*main)(), char *task_name) {
    asm volatile("movq %%cr3, %%rax; movq %%rax, %0;" : "=m"(temp.regs.cr3)::"%rax");
    task->regs.rax = 0;
    task->regs.rbx = 0;
    task->regs.rcx = 0;
    task->regs.rdx = 0;
    task->regs.rsi = 0;
    task->regs.rdi = 0;
    task->regs.r8 = 0;
    task->regs.r9 = 0;
    task->regs.r10 = 0;
    task->regs.r11 = 0;
    task->regs.r12 = 0;
    task->regs.r13 = 0;
    task->regs.r14 = 0;
    task->regs.r15 = 0;
    task->regs.rflags = temp.regs.rflags;
    task->regs.rip = (uint64_t)main;
    task->regs.cr3 = (uint64_t)temp.regs.cr3;
    task->regs.rsp = ((uint64_t)kmalloc(0x4000) & (~0xf)) +
        0x4000; // Allocate 16KB for the process stack
    task->start_esp = (uint8_t *)task->regs.rsp - 0x4000;
    task->scancode_buffer = kmalloc(512); // 512 chars for each task's keyboard buffer
    task->regs.rbp = 0;
    task->cursor_pos = get_cursor_offset();
    task->buffer = current_buffer;
    strcpy((char *)&task->name, task_name);

    task->next = main_task.next;
    task->next_dead = 0;
    main_task.next = task;
    task->pid = pid_max;
    task->ticks_cpu_time = 0;
    task->state = RUNNING;
    task->priority = NORMAL;
    pid_max++;
    Task *iterator = (&main_task)->next;
    while (iterator->pid != 0) {
        iterator->since_last_task = 0;
        iterator = iterator->next;
    }
    iterator->since_last_task = 0;

    return task->pid;
}

void yield() {
    time_slice_left = 1;
    asm volatile("int $32"); // Changes task
}

void schedule_task(registers_t *r) {
    // Set values for context switch
    regs = &running_task->regs;
    r->rflags = regs->rflags | 0x200;
    r->rax = regs->rax;
    r->rbx = regs->rbx;
    r->rcx = regs->rcx;
    r->rdx = regs->rdx;
    r->rdi = regs->rdi;
    r->rsi = regs->rsi;
    r->rip = regs->rip;
    r->rbp = regs->rbp;
    r->r8 = regs->r8;
    r->r9 = regs->r9;
    r->r10 = regs->r10;
    r->r11 = regs->r11;
    r->r12 = regs->r12;
    r->r13 = regs->r13;
    r->r14 = regs->r14;
    r->r15 = regs->r15;
    if (running_task->cursor_pos < CURSOR_MAX) {
        set_cursor_offset(running_task->cursor_pos);
    }
    current_buffer = running_task->buffer;
}

void sprint_tasks() {
    Task *temp_list = &main_task;
    uint32_t found_task = 1;
    uint32_t loop = 0;
    while (1) {
        if (loop > pid_max) {
            break;
        }
        if (temp_list->pid == 0 && found_task != 1) {
            break;
        }
        sprint("\n");
        sprint_uint(temp_list->pid);
        sprint("  ");
        sprint_uint(temp_list->ticks_cpu_time);
        sprint("  ");
        sprint(temp_list->name);
        temp_list = temp_list->next;
        found_task = 0;
        loop++;
    }
}

int32_t kill_task(uint32_t pid) {
    if (pid == 0 || pid == 1 || pid == 2) {
        return 2; // Permission denied
    }
    sprintf("\nKilling PID: %u", pid);
    if (pid == running_task->pid) {
        sprintf("\nBad code eeeee PID: %u", pid);
    }
    Task *after_target = &main_task;
    Task *to_kill = 0;
    Task *before_target;
    uint32_t loop = 0;
    while (1) {
        if (loop > pid_max) {
            break;
        }
        if (after_target->next->pid == pid) {
            to_kill = after_target->next; // Process to kill
            before_target = after_target; // Process before
            after_target = after_target->next->next; // Process after
            break;
        }
        if (after_target->next->pid == 0) {
            return 1; // Task not found, looped back to 0
        }
        loop++;
        after_target = after_target->next;
    }
    if ((uint64_t)to_kill == 0) {
        return 1; // This should never be reached but eh, safety first
    }
    if (pid != running_task->pid) {
        free(to_kill->start_esp);
    }
    free(to_kill->scancode_buffer);
    before_target->next = after_target; // Remove killed task from the chain
    free(to_kill);
    return 0; // Worked... hopefully lol
}

void print_tasks() {
    Task *temp_list = &main_task;
    uint32_t oof = 1;
    uint32_t loop = 0;
    while (1) {
        if (loop > pid_max) {
            break;
        }
        if (temp_list->pid == 0 && oof != 1) {
            break;
        }
        kprint("\n");
        kprint_uint(temp_list->pid);
        kprint("  ");
        kprint_uint(temp_list->ticks_cpu_time);
        kprint("  ");
        kprint(temp_list->name);
        temp_list = temp_list->next;
        oof = 0;
        loop++;
    }
}

/* Task selector */
void pick_task() {
    // Select new running task
    uint32_t lowest_time = 0xffffffff;
    Task *lowest_time_task = (&main_task);
    Task *temp_iterator = (&main_task)->next;

    while (temp_iterator->pid != 0) {
        // sprintf("\nLowest time: %x Lowest time name: %s\nIterator name: %s", lowest_time,
        // lowest_time_task->name, temp_iterator->name);
        if ((temp_iterator->since_last_task < lowest_time) &&
            temp_iterator->state == RUNNING) {
            lowest_time = temp_iterator->since_last_task;
            lowest_time_task = temp_iterator;
        }
        temp_iterator = temp_iterator->next;
    }
    // sprintf("\nPID: %u\nState: %u", lowest_time_task->pid, (uint32_t)
    // lowest_time_task->state);
    if (lowest_time_task->state != RUNNING) {
        sprintf("\nError");
        while (1) {
            asm volatile("int $22"); // yes
        }
    }
    running_task = lowest_time_task;
    running_task->since_last_task += 1; // Times selected since last task started
}

void swap_task(registers_t *r) {
    regs = &running_task->regs; // Get registers
    /* Set old registers */
    regs->rflags = r->rflags;
    regs->rax = r->rax;
    regs->rbx = r->rbx;
    regs->rcx = r->rcx;
    regs->rdx = r->rdx;
    regs->rdi = r->rdi;
    regs->rsi = r->rsi;
    regs->rip = r->rip;
    // sprintf("\nOld RIP: %lx", regs->rip);
    regs->rsp = r->rsp;
    regs->rbp = r->rbp;
    regs->r8 = r->r8;
    regs->r9 = r->r9;
    regs->r10 = r->r10;
    regs->r11 = r->r11;
    regs->r12 = r->r12;
    regs->r13 = r->r13;
    regs->r14 = r->r14;
    regs->r15 = r->r15;
    if (running_task->cursor_pos < CURSOR_MAX) {
        running_task->cursor_pos = get_cursor_offset();
    }
    running_task->buffer = current_buffer;
    pick_task();
    regs = &running_task->regs; // Get registers again
    /* Set new registers for the interrupt frame */
    r->rflags = regs->rflags;
    r->rax = regs->rax;
    r->rbx = regs->rbx;
    r->rcx = regs->rcx;
    r->rdx = regs->rdx;
    r->rdi = regs->rdi;
    r->rsi = regs->rsi;
    r->rbp = regs->rbp;
    r->rsp = regs->rsp;
    r->rip = regs->rip;
    // sprintf("\nNew RIP: %lx", r->rip);
    r->r8 = regs->r8;
    r->r9 = regs->r9;
    r->r10 = regs->r10;
    r->r11 = regs->r11;
    r->r12 = regs->r12;
    r->r13 = regs->r13;
    r->r14 = regs->r14;
    r->r15 = regs->r15;
    if (running_task->cursor_pos < CURSOR_MAX) {
        set_cursor_offset(running_task->cursor_pos);
    }
    current_buffer = running_task->buffer;
}