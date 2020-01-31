#include "syscall.h"
#include "task.h"


void syscall_handle(registers_t *r) {
    uint16_t al = (uint16_t)(r->rax & 0xffff);
    // uint16_t ah = (uint16_t)((r->eax & 0xffff0000) >> 16);
    sprint("\nsyscall ");
    sprint_uint(al);
    sprintf("\nBy task %u", running_task->pid);
    if (al == 0) {
        /* Exit syscall */
        running_task->state = DEAD_TASK;
        //sprintf("\nState: %u Pid: %u", (uint8_t) running_task->state, running_task->pid);
        if (dead_task_queue) {
            Task *queue_iterator = dead_task_queue;
            while (queue_iterator->next_dead) {
                //sprintf("\nLoop");
                queue_iterator = queue_iterator->next_dead;
            }
            queue_iterator->next_dead = running_task;
        } else {
            dead_task_queue = running_task;
        }
        switch_task = 1;
        //sprintf("\nEscaped!");
        return; // bye bye :P
    }
}