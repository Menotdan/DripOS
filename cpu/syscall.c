#include "syscall.h"
#include "task.h"


void syscall_handle(registers_t *r) {
    uint16_t al = (uint16_t)(r->rax & 0xffff);
    // uint16_t ah = (uint16_t)((r->eax & 0xffff0000) >> 16);
    sprint("\nsyscall ");
    sprint_uint(al);
    if (al == 0) {
        /* Exit syscall */
        kill_task(running_task->pid); // TODO: Prevent anyone else from getting the
        // stack memory, so that it can be safely discarded when we are done
        global_esp_old = (uint64_t)running_task->start_esp;
        global_old_task = running_task;
        pick_task();
        // We can now discard the stack memory
        global_esp = running_task->regs.rsp;
        switch_task = 1;
        
        return; // bye bye
    }
}