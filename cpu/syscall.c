#include "syscall.h"
#include "task.h"



void syscall_handle(registers_t *r) {
    uint16_t al = (uint16_t)(r->eax & 0xffff);
    // uint16_t ah = (uint16_t)((r->eax & 0xffff0000) >> 16);

    if (al == 0) {
        // Exit call
        pick_task();
        schedule_task(r);
    }
}