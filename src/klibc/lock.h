#ifndef KLIBC_LOCK_H
#define KLIBC_LOCK_H
#include <stdint.h>

typedef volatile uint32_t lock_t;

#ifdef AMD64
extern void spinlock_lock(lock_t *lock);
extern void spinlock_unlock(lock_t *lock);
extern uint32_t atomic_inc(lock_t *data);
extern uint32_t atomic_dec(lock_t *data);
#else
extern void spinlock_lock(lock_t *lock);
extern void spinlock_unlock(lock_t *lock);
extern uint32_t atomic_inc(lock_t *data);
extern uint32_t atomic_dec(lock_t *data);
#endif

void lock(lock_t *lock);
void unlock(lock_t *lock);

void interrupt_lock();
void interrupt_unlock();
uint8_t check_interrupts();

#endif