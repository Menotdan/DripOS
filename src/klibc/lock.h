#ifndef KLIBC_LOCK_H
#define KLIBC_LOCK_H
#include <stdint.h>

typedef struct {
    uint32_t lock_dat;
    uint64_t current_holder;
} lock_t;

#ifdef AMD64
extern void spinlock_lock(uint32_t *lock);
extern void spinlock_unlock(uint32_t *lock);
extern uint32_t atomic_inc(uint32_t *data);
extern uint32_t atomic_dec(uint32_t *data);
#else
extern void spinlock_lock(uint32_t *lock);
extern void spinlock_unlock(uint32_t *lock);
extern uint32_t atomic_inc(uint32_t *data);
extern uint32_t atomic_dec(uint32_t *data);
#endif

#define lock(lock_) \
    spinlock_lock(&lock_.lock_dat);
#define unlock(lock_) \
    spinlock_unlock(&lock_.lock_dat);
void interrupt_lock();
void interrupt_unlock();
uint8_t check_interrupts();

#endif