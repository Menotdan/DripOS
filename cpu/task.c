#include "task.h"

Task *runningTask;
Task mainTask;
Task otherTask;
Task kickstart;
static Task temp;
Task *next_temp;
uint32_t pid_max = 0;
registers_t *global_regs;
uint32_t call_counter = 0;
Registers *regs;
registers_t *temp_data1;
uint32_t oof = 0;
uint32_t eax = 0;
uint32_t eip = 0;
uint32_t esp = 0;
uint32_t just_started = 1;

static void otherMain() {
    loaded = 1;
    while (1) {
        //oof++;
        //sprint("\n");
        //sprint_uint(oof);
        asm volatile("hlt");
    }
}

void initTasking() {
    // Get EFLAGS and CR3
    asm volatile("movl %%cr3, %%eax; movl %%eax, %0;":"=m"(temp.regs.cr3)::"%eax"); // No paging yet
    asm volatile("pushfl; movl (%%esp), %%eax; movl %%eax, %0; popfl;":"=m"(temp.regs.eflags)::"%eax");

    createTask(&mainTask, otherMain, "Idle task");
    createTask(&kickstart, 0, "no");
    //Log("Initializing terminal", 1);
	//Log("Terminal loaded", 3);
    pid_max = 1;
    mainTask.next = &mainTask;
    kickstart.next = &mainTask;
    mainTask.cursor_pos = CURSOR_MAX;
    init_terminal();
    global_regs = kmalloc(sizeof(registers_t));
    runningTask = &kickstart;
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
    task->regs.ebp = 0;
    task->cursor_pos = get_cursor_offset();
    task->screen = current_screen;

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
    memory_copy((uint8_t *)&tempregs, stack_insert_buffer, sizeof(registers_t));
    //breakA();

    next_temp = mainTask.next;
    task->next = next_temp;
    mainTask.next = task;
    task->pid = pid_max;
    task->ticks_cpu_time = 0;
    task->state = RUNNING;
    task->priority = NORMAL;
    pid_max++;
    return task->pid;
}

void yield() {
    timesliceleft = 1;
    asm volatile("int $32"); // Changes task
}

int32_t kill_task(uint32_t pid) {
    if (pid == 0 || pid == 1) {
        return 2; // Permission denied
    }
    Task *temp_kill = &mainTask;
    Task *to_kill = 0;
    Task *prev_task;
    uint32_t loop = 0;
    //kprint("\nPID to find: ");
    //kprint_uint(pid);
    //kprint("\nMax PID: ");
    //kprint_uint(pid_max);
    while (1) {
        //kprint("\nLoop: ");
        //kprint_uint(loop);
        if (loop > pid_max) {
            break;
        }
        //kprint("\nCurrent pid: ");
        //kprint_uint(temp_kill->next->pid);
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
    prev_task->next = temp_kill; // Remove killed task from the chain
    return 0; // Worked... hopefully lol
}

void print_tasks() {
    Task *temp_list = &mainTask;
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

void timer_switch_task(registers_t *from, Task *to) {
    //free(test, 0x1000);
    Registers *regs = &runningTask->regs;
    regs->eflags = from->eflags;
    regs->eax = from->eax;
    regs->ebx = from->ebx;
    regs->ecx = from->ecx;
    regs->edx = from->edx;
    regs->edi = from->edi;
    regs->esi = from->esi;
    regs->eip = from->eip;
    regs->esp = from->esp;
    regs->ebp = from->ebp;
    runningTask = to;
}

void schedule(registers_t *from) {
    Registers *regs = &runningTask->regs; // Get registers
    /* Set old registers */
    regs->eflags = from->eflags;
    regs->eax = from->eax;
    regs->ebx = from->ebx;
    regs->ecx = from->ecx;
    regs->edx = from->edx;
    regs->edi = from->edi;
    regs->esi = from->esi;
    regs->eip = from->eip;
    regs->esp = from->esp;
    regs->ebp = from->ebp;
    // Select new running task
    runningTask = runningTask->next;
    while (runningTask->state != RUNNING) {
        runningTask = runningTask->next;
    }
}

/* Task selector */
void pick_task() {
    // Select new running task
    runningTask = runningTask->next;
    while (runningTask->state != RUNNING) {
        runningTask = runningTask->next;
    }
    if (runningTask->pid == 0) {
        while (runningTask->state != RUNNING) {
            runningTask = runningTask->next;
        }
    }
}

void store_values(registers_t *r) {
    regs = &runningTask->regs; // Get registers
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
    if (runningTask->cursor_pos < CURSOR_MAX)  {
        runningTask->cursor_pos = get_cursor_offset();
    }
    pick_task();
    regs = &runningTask->regs; // Get registers again
    call_counter = regs->esp;
}

void schedule_task(registers_t *r) {
    // Set values for context switch
    regs = &runningTask->regs;
    r->eflags = regs->eflags | 0x200;
    r->eax = regs->eax;
    r->ebx = regs->ebx;
    r->ecx = regs->ecx;
    r->edx = regs->edx;
    r->edi = regs->edi;
    r->esi = regs->esi;
    r->eip = regs->eip;
    r->ebp = regs->ebp;
    if (runningTask->cursor_pos < CURSOR_MAX) {
        set_cursor_offset(runningTask->cursor_pos);
    }
}

void irq_schedule() {
    regs = &runningTask->regs; // Get registers
    /* Set old registers */
    regs->eflags = global_regs->eflags;
    regs->eax = global_regs->eax;
    regs->ebx = global_regs->ebx;
    regs->ecx = global_regs->ecx;
    regs->edx = global_regs->edx;
    regs->edi = global_regs->edi;
    regs->esi = global_regs->esi;
    regs->eip = global_regs->eip;
    regs->esp = global_regs->esp;
    regs->ebp = global_regs->ebp;
    // Select new running task
    runningTask = runningTask->next;
    while (runningTask->state != RUNNING) {
        runningTask = runningTask->next;
    }
    // Switch
    //sprint_uint(3);
    regs = &(runningTask->regs);
    sprint("\nFrom eip: ");
    sprint_uint(global_regs->eip);
    sprint("\nFrom esp: ");
    sprint_uint(global_regs->esp);
    sprint("\nRunning eip: ");
    sprint_uint(runningTask->regs.eip);
    sprint("\nRunning esp: ");
    sprint_uint(runningTask->regs.esp);
    oof = (uint32_t)&(runningTask->regs);
    sprint("\nOOF: ");
    sprint_uint(oof);
    switchTask();
}
void store_global(uint32_t f, registers_t *ok) {
    sprint("\nEIP: ");
    sprint_uint(ok->eip);
    sprint("\nEAX: ");
    sprint_uint(ok->eax);
    sprint("\nEBX: ");
    sprint_uint(ok->ebx);
    sprint("\nEDX: ");
    sprint_uint(ok->edx);
    sprint("\nESP: ");
    sprint_uint(ok->esp);
    sprint("\nADDR: ");
    sprint_uint((uint32_t)ok);
    sprint("\nF: ");
    sprint_uint((uint32_t)f);
    
    /* Set all 17 */
    //global_regs->dr6 = f;
    global_regs->cs = ok->cs;
    global_regs->ds = ok->ds;
    global_regs->ecx = ok->ecx;
    global_regs->eax = ok->eax;
    global_regs->ebx = ok->ebx;
    global_regs->edx = ok->edx;
    global_regs->esp = ok->esp;
    global_regs->ebp = ok->ebp;
    global_regs->eip = ok->eip;
    global_regs->esi = ok->esi;
    global_regs->eflags = ok->eflags;
    global_regs->edi = ok->edi;
    //global_regs->useresp = ok->useresp;
    global_regs->err_code = ok->err_code;
    //global_regs->ss = ok->ss;
    global_regs->int_no = ok->int_no;
    global_regs->dr6 = ok->dr6;
}

void print_stuff() {
    sprint("\nMade it to the end... this is it I guess...");
}