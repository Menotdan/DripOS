#include "lock.h"
#include "proc/scheduler.h"
#include "klibc/stdlib.h"

#include "drivers/serial.h"

void lock(lock_t *lock) {
    spinlock_lock(lock);

}

void unlock(lock_t *lock) {
    spinlock_unlock(lock);
}

void interrupt_lock() {
    if (scheduler_started) {
        asm volatile("cli");
    }
}

void interrupt_unlock() {
    if (scheduler_started) {
        asm volatile("sti");
    }
}

uint8_t check_interrupts() {
    if (!scheduler_started) {
        return 0; // Pretend that interrupts are off so we don't yield
    }
    uint64_t rflags;
    asm volatile("pushfq; pop %%rax; movq %%rax, %0;" : "=r"(rflags) :: "rax");
    uint8_t int_flag = (uint8_t) ((rflags & (0x200)) >> 9);
    return int_flag;
}