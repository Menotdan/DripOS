#include "lock.h"
#include "proc/scheduler.h"
#include "klibc/stdlib.h"
#include "klibc/kern_state.h"

#include "drivers/tty/tty.h"
#include "drivers/serial.h"

void lock(lock_t *lock) {
    // uint8_t lock_print_state;

    // if (load_kernel_state("lock_print_addr", &lock_print_state, 1) && lock_print_state) {
    //     sprintf("\nLock %lx", lock);
    // }
    spinlock_lock(lock);
}

void unlock(lock_t *lock) {
    // uint8_t lock_print_state;

    // if (load_kernel_state("lock_print_addr", &lock_print_state, 1) && lock_print_state) {
    //     sprintf("\nUnlock %lx", lock);
    // }
    spinlock_unlock(lock);
}

void interrupt_lock() {
    if (scheduler_enabled) {
        asm volatile("cli");
    }
}

void interrupt_unlock() {
    if (scheduler_enabled) {
        asm volatile("sti");
    }
}

uint8_t check_interrupts() {
    if (!scheduler_enabled) {
        return 0; // Pretend that interrupts are off so we don't yield
    }
    uint64_t rflags;
    asm volatile("pushfq; pop %%rax; movq %%rax, %0;" : "=r"(rflags) :: "rax");
    uint8_t int_flag = (uint8_t) ((rflags & (0x200)) >> 9);
    return int_flag;
}

void deadlock_possible() {
    kprintf("\nfound possible deadlock!");
}