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
    sprintf("\nWarning: Potential deadlock in a lock held by %s", lock->current_holder);
    sprintf("\nAttempting to get lock from %s", lock->attempting_to_get);
}