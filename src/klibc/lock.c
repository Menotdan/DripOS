#include "lock.h"
#include "proc/scheduler.h"
#include "klibc/stdlib.h"

#include "drivers/serial.h"

void lock(uint32_t *lock) {
    spinlock_lock(lock);
}

void unlock(uint32_t *lock) {
    spinlock_unlock(lock);
}

void interrupt_lock() {
    if (scheduler_start) {
        asm volatile("cli");
    }
}

void interrupt_unlock() {
    if (scheduler_start) {
        asm volatile("sti");
    }
}

uint8_t check_interrupts() {
    if (!scheduler_start) {
        return 0; // Pretend that interrupts are off so we don't yield
    }
    uint64_t rflags;
    asm volatile("pushfq; pop %%rax; movq %%rax, %0;" : "=r"(rflags) :: "rax");
    sprintf("\nInterrupt flags: %lx", rflags);
    uint8_t int_flag = (uint8_t) ((rflags & (1<<9)) >> 9);
    sprintf("\nFlag: %u", (uint32_t) int_flag);
    return int_flag;
}