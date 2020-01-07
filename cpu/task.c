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

void new_stack(uint32_t address) {
    asm __volatile__("xchg %%esp, %0\n\t"
        : "=r"(old_stack_ptr)
        : "0"(address)
        );
}

void initTasking() {
    // Get EFLAGS and CR3
    asm volatile("movl %%cr3, %%eax; movl %%eax, %0;":"=m"(temp.regs.cr3)::"%eax"); // No paging yet
    asm volatile("pushfl; movl (%%esp), %%eax; movl %%eax, %0; popfl;":"=m"(temp.regs.eflags)::"%eax");

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
    asm volatile("movl %%cr3, %%eax; movl %%eax, %0;":"=m"(temp.regs.cr3)::"%eax"); // No paging yet
    asm volatile("pushfl; movl (%%esp), %%eax; movl %%eax, %0; popfl;":"=m"(temp.regs.eflags)::"%eax");
    task->regs.eax = 0;
    task->regs.ebx = 0;
    task->regs.ecx = 0;
    task->regs.edx = 0;
    task->regs.esi = 0;
    task->regs.edi = 0;
    task->regs.eflags = temp.regs.eflags;
    task->regs.eip = (uint32_t) main;
    task->regs.cr3 = (uint32_t)temp.regs.cr3; // No paging yet
    task->regs.esp = ((uint32_t)kmalloc(0x4000) & (~0xf)) + 0x4000; // Allocate 16KB for the process stack
    task->start_esp = (uint8_t *)task->regs.esp - 0x4000;
    task->scancode_buffer = kmalloc(512); // 512 chars for each task's keyboard buffer
    task->regs.ebp = 0;
    task->cursor_pos = get_cursor_offset();
    task->buffer = current_buffer;
    

    strcpy((char *)&task->name, task_name);

    registers_t tempregs;
    tempregs.eax = 0;
    tempregs.ebx = 0;
    tempregs.ecx = 0;
    tempregs.edx = 0;
    tempregs.esi = 0;
    tempregs.edi = 0;
    tempregs.eflags = temp.regs.eflags;
    tempregs.eip = (uint32_t) main;
    tempregs.ebp = 0;
    tempregs.cs = 0x8;
    tempregs.ds = 0x10;
    tempregs.esp = task->regs.esp;
    tempregs.dr6 = 0;
    tempregs.err_code = 0;
    tempregs.int_no = 1234;
    task->regs.esp -= sizeof(registers_t);
    uint8_t *stack_insert_buffer = get_pointer(task->regs.esp);
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
    r->eflags = regs->eflags | 0x200;
    r->eax = regs->eax;
    r->ebx = regs->ebx;
    r->ecx = regs->ecx;
    r->edx = regs->edx;
    r->edi = regs->edi;
    r->esi = regs->esi;
    r->eip = regs->eip;
    r->ebp = regs->ebp;
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
    if ((uint32_t)to_kill == 0) {
        return 1; // This should never be reached but eh, safety first
    }
    if (pid != running_task->pid) {
        free(to_kill->start_esp, 0x4000);
    }
    free(to_kill->scancode_buffer, 512);
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
    regs->eflags = r->eflags;
    regs->eax = r->eax;
    regs->ebx = r->ebx;
    regs->ecx = r->ecx;
    regs->edx = r->edx;
    regs->edi = r->edi;
    regs->esi = r->esi;
    regs->eip = r->eip;
    regs->esp = r->esp-40;
    regs->ebp = r->ebp;
    if (running_task->cursor_pos < CURSOR_MAX)  {
        running_task->cursor_pos = get_cursor_offset();
    }
    running_task->buffer = current_buffer;
    pick_task();
    regs = &running_task->regs; // Get registers again
    global_esp = regs->esp;
}