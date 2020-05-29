#ifndef KLIBC_LOCK_H
#define KLIBC_LOCK_H
#include <stdint.h>
#include "drivers/serial.h"
#include "klibc/string.h"

typedef uint8_t interrupt_state_t;

typedef volatile struct {
    uint32_t lock_dat;
    const char *current_holder;
    const char *attempting_to_get;
    interrupt_state_t interrupt_state;
} lock_t;

extern void spinlock_lock(volatile uint32_t *lock);
extern void spinlock_unlock(volatile uint32_t *lock);
extern uint64_t spinlock_check_and_lock(volatile uint32_t *lock);
extern uint32_t atomic_inc(volatile uint32_t *data);
extern uint32_t atomic_dec(volatile uint32_t *data);

interrupt_state_t interrupt_lock();
void interrupt_unlock(interrupt_state_t state);
uint8_t check_interrupts();

#define lock(lock_) \
    lock_.attempting_to_get = __FUNCTION__; \
    spinlock_lock(&lock_.lock_dat); \
    lock_.current_holder = __FUNCTION__;
#define unlock(lock_) \
    spinlock_unlock(&lock_.lock_dat);

#endif