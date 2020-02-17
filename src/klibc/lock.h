#ifndef KLIBC_LOCK_H
#define KLIBC_LOCK_H
#include <stdint.h>

typedef uint32_t lock_t;

#ifdef AMD64
extern void spinlock_lock(uint32_t *lock);
extern void spinlock_unlock(uint32_t *lock);
extern atomic_inc(uint32_t *data);
extern atomic_dec(uint32_t *data);
#else
extern void spinlock_lock(uint32_t *lock);
extern void spinlock_unlock(uint32_t *lock);
extern atomic_inc(uint32_t *data);
extern atomic_dec(uint32_t *data);
#endif

#endif