#include "lock.h"
#include "proc/scheduler.h"
#include "klibc/stdlib.h"

#include "drivers/tty/tty.h"
#include "drivers/serial.h"


interrupt_state_t interrupt_lock() {
    interrupt_state_t ret = (interrupt_state_t) check_interrupts();
    asm volatile("cli");
    return ret;
}

void interrupt_unlock(interrupt_state_t state) {
    if (state) {
        asm volatile("sti");
    }
}

uint8_t check_interrupts() {
    uint64_t rflags;
    asm volatile("pushfq; pop %%rax; movq %%rax, %0;" : "=r"(rflags) :: "rax");
    uint8_t int_flag = (uint8_t) ((rflags & (0x200)) >> 9);
    return int_flag;
}

void deadlock_handler(lock_t *lock) {
    sprintf("Warning: Potential deadlock in lock %s held by %s\n", lock->lock_name, lock->current_holder);
    sprintf("Attempting to get lock from %s\n", lock->attempting_to_get);
    sprintf("Process count: %lu\n", process_count);
}