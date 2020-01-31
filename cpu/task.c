#include "task.h"

Task *running_task;
Task main_task;
Task kickstart;
static Task temp;
Task *next_temp;
uint32_t old_stack_ptr;
uint32_t pid_max = 0;
uint32_t global_esp = 0;
uint32_t global_esp_old = 0;
uint32_t task_size = sizeof(Task);
Task *global_old_task;
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
    while (1) {
        asm volatile("hlt");
    }
}

void set_focused_task(Task *new_focus) {
    focus_tasks = new_focus;
}

Task *get_focused_task() {
    return focus_tasks;
}

void initTasking() {
    // Get EFLAGS and CR3
    asm volatile("movq %%cr3, %%rax; movq %%rax, %0;":"=m"(temp.regs.cr3)::"%rax"); // No paging yet
    asm volatile("pushfq; movq (%%rsp), %%rax; movq %%rax, %0; popfq;":"=m"(temp.regs.rflags)::"%rax");

    createTask(&main_task, otherMain, "Idle task");
    createTask(&kickstart, 0, "no");
    pid_max = 1;
    main_task.next = &main_task;
    kickstart.next = &main_task;
    main_task.cursor_pos = CURSOR_MAX;
    Task *temp = kmalloc(sizeof(Task));
    //createTask(temp, task_cleaner, "Task handler");
    temp->cursor_pos = CURSOR_MAX;

    init_terminal();
    running_task = &kickstart;
    otherMain();
}
 
uint32_t createTask(Task *task, void (*main)(), char *task_name) {//, uint32_t *pagedir) { // No paging yet
    asm volatile("movq %%cr3, %%rax; movq %%rax, %0;":"=m"(temp.regs.cr3)::"%rax");
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
    task->regs.rip = (uint64_t) main;
    task->regs.cr3 = (uint64_t)temp.regs.cr3;
    task->regs.rsp = ((uint64_t)kmalloc(0x4000) & (~0xf)) + 0x4000; // Allocate 16KB for the process stack
    task->start_esp = (uint8_t *)task->regs.rsp - 0x4000;
    task->scancode_buffer = kmalloc(512); // 512 chars for each task's keyboard buffer
    task->regs.rbp = 0;
    task->cursor_pos = get_cursor_offset();
    task->buffer = current_buffer;
    

    strcpy((char *)&task->name, task_name);

    registers_t tempregs;
    tempregs.rax = 0;
    tempregs.rbx = 0;
    tempregs.rcx = 0;
    tempregs.rdx = 0;
    tempregs.rsi = 0;
    tempregs.rdi = 0;
    tempregs.r8 = 0;
    tempregs.r9 = 0;
    tempregs.r10 = 0;
    tempregs.r11 = 0;
    tempregs.r12 = 0;
    tempregs.r13 = 0;
    tempregs.r14 = 0;
    tempregs.r15 = 0;
    tempregs.rflags = temp.regs.rflags;
    tempregs.rip = (uint64_t) main;
    tempregs.rbp = 0;
    tempregs.cs = 0x8;
    tempregs.ds = 0x10;
    tempregs.rsp = task->regs.rsp;
    tempregs.dr6 = 0;
    tempregs.err_code = 0;
    tempregs.int_no = 1234;
    task->regs.rsp -= sizeof(registers_t);
    uint8_t *stack_insert_buffer = get_pointer(task->regs.rsp);
    memcpy((uint8_t *)&tempregs, stack_insert_buffer, sizeof(registers_t));
    //breakA();

    next_temp = main_task.next;
    task->next = next_temp;
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
    uint32_t oof = 1;
    uint32_t loop = 0;
    while (1) {
        if (loop > pid_max) {
            break;
        }
        if (temp_list->pid == 0 && oof != 1) {
            break;
        }
        sprint("\n");
        sprint_uint(temp_list->pid);
        sprint("  ");
        sprint_uint(temp_list->ticks_cpu_time);
        sprint("  ");
        sprint(temp_list->name);
        temp_list = temp_list->next;
        oof = 0;
        loop++;
    }
}

int32_t kill_task(uint32_t pid) {
    if (pid == 0 || pid == 1) {
        return 2; // Permission denied
    }
    Task *temp_kill = &main_task;
    Task *to_kill = 0;
    Task *prev_task;
    uint32_t loop = 0;
    while (1) {
        if (loop > pid_max) {
            break;
        }
        if (temp_kill->next->pid == pid) {
            to_kill = temp_kill->next; // Process to kill
            prev_task = temp_kill; // Process before
            temp_kill = temp_kill->next->next; // Process after
            break;
        }
        if (temp_kill->next->pid == 0) {
            return 1; // Task not found, looped back to 0
        }
        loop++;
        temp_kill = temp_kill->next;
    }
    if ((uint64_t)to_kill == 0) {
        return 1; // This should never be reached but eh, safety first
    }
    if (pid != running_task->pid) {
        free(to_kill->start_esp);
    }
    free(to_kill->scancode_buffer);
    prev_task->next = temp_kill; // Remove killed task from the chain
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
        if ((temp_iterator->since_last_task < lowest_time) && temp_iterator->state == RUNNING) {
            lowest_time = temp_iterator->since_last_task;
            lowest_time_task = temp_iterator;
        }
        temp_iterator = temp_iterator->next;
    }
    running_task = lowest_time_task;
    running_task->since_last_task += 1; // Times selected since last task started
}

void store_values(registers_t *r) {
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
    regs->rsp = r->rsp-40;
    regs->rbp = r->rbp;
    regs->r8 = r->r8;
    regs->r9 = r->r9;
    regs->r10 = r->r10;
    regs->r11 = r->r11;
    regs->r12 = r->r12;
    regs->r13 = r->r13;
    regs->r14 = r->r14;
    regs->r15 = r->r15;
    if (running_task->cursor_pos < CURSOR_MAX)  {
        running_task->cursor_pos = get_cursor_offset();
    }
    running_task->buffer = current_buffer;
    pick_task();
    regs = &running_task->regs; // Get registers again
    global_esp = regs->rsp;
}