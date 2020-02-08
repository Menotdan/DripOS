#ifndef KLIBC_LOCK_H
#define KLIBC_LOCK_H
#include <stdint.h>

typedef uint32_t lock_t;

#ifdef AMD64
extern void spinlock_lock(uint32_t *lock);
extern void spinlock_unlock(uint32_t *lock);
#else
extern void spinlock_lock(uint32_t *lock);
extern void spinlock_unlock(uint32_t *lock);
#endif

#endif