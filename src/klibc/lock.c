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
        yield();
    }
}