#include "lock.h"
#include "proc/scheduler.h"

void lock(uint32_t *lock) {
    if (started) {
        asm volatile("cli");
    }
    spinlock_lock(lock);
}

void unlock(uint32_t *lock) {
    spinlock_unlock(lock);
    if (started) {
        asm volatile("sti");
    }
}