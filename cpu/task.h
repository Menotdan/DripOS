#include <stdint.h>
#include "isr.h"
#include "../drivers/vesa.h"
#include "../libc/mem.h"
#include "../drivers/screen.h"
#include "../libc/string.h"
#include "../kernel/kernel.h"
#include "../kernel/terminal.h"
#include "../cpu/isr.h"
#include "../drivers/serial.h"
#include "../drivers/sound.h"
#include <debug.h>
#include "timer.h"

#ifndef TASK_H
#define TASK_H

/* Task states */
#define RUNNING 0
#define BLOCKED 1
#define SLEEPING 2
#define IRQ_WAIT 3
/* Priorities */
#define HIGH 3
#define NORMAL 2
#define LOW 1
#define VERY_LOW 0
/* Cursor max (cursor pos wont be updated for tasks with a cursor_pos greater than this) */
#define CURSOR_MAX 4000000000
extern void initTasking();
 
typedef struct {
// eax off   0    4    8    12   16   20   24   28   32   36      40
    uint32_t eax, ebx, ecx, edx, esi, edi, esp, ebp, eip, eflags, cr3;
} __attribute__((__packed__)) Registers;
 
typedef struct Task {
    Registers regs;
    uint8_t *start_esp; // The original allocated memory for the stack of this process
    uint32_t ticks_cpu_time;
    struct Task *next;
    uint8_t priority;
    uint32_t pid; // Process id of the task
    uint8_t state; // The state the task is in
    uint32_t waiting; // If state is SLEEPING, this is the tick to restart the task, if the state is IRQ_WAIT, this is the IRQ that it is waiting for
    uint32_t cursor_pos;
    vesa_tty_t screen;
    char name[21];
} Task;

void initTasking();
extern uint32_t createTask(Task *task, void (*main)(), char *task_name);
extern int32_t kill_task(uint32_t pid); // 
extern void yield(); // Yield, will be optional
extern void switchTask(); // The function which actually switches
extern void print_tasks();
extern void timer_switch_task(registers_t *from, Task *to);
Task *running_task;
void store_global(uint32_t f, registers_t *ok);
void irq_schedule();
uint32_t call_counter;
uint32_t oof;
uint32_t eax;
uint32_t eip;
uint32_t esp;
registers_t *temp_data1;
Task mainTask;
#endif