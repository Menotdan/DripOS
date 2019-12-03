#pragma once
#include <stdint.h>

extern void initTasking();
 
typedef struct {
    uint32_t eax, ebx, ecx, edx, esi, edi, esp, ebp, eip, eflags, cr3;
} Registers;
 
typedef struct Task {
    Registers regs;
    uint32_t ticks_cpu_time;
    struct Task *next;
    uint32_t pid;
} Task;
 
void initTasking();
extern uint32_t createTask(Task *task, void (*main)());//, uint32_t*); No paging yet
extern int32_t kill_task(uint32_t pid);
extern void yield(); // Switch task frontend
extern void switchTask(Registers *old, Registers *new); // The function which actually switches
Task *runningTask;