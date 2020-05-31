#ifndef KLIBC_LOCK_H
#define KLIBC_LOCK_H
#include <stdint.h>
#include "drivers/serial.h"
#include "klibc/string.h"
#include "sys/cpu_index.h"

typedef uint8_t interrupt_state_t;

typedef volatile struct {
    uint32_t lock_dat;
    const char *current_holder;
    const char *attempting_to_get;
    const char *lock_name;
} lock_t;

typedef volatile struct {
    uint32_t lock_dat;
    const char *current_holder;
    const char *attempting_to_get;
    const char *lock_name;
    int cpu_holding_lock;
} interrupt_safe_lock_t;

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
    lock_.lock_name = #lock_; \
    spinlock_lock(&lock_.lock_dat); \
    lock_.current_holder = __FUNCTION__;
#define unlock(lock_) \
    spinlock_unlock(&lock_.lock_dat);

        // if (lock_.cpu_holding_lock != -1 && lock_.cpu_holding_lock != get_cpu_index()) { 
        //     spinlock_lock(&lock_.lock_dat); 
        //     ret = 1; 
        // } 


#define interrupt_safe_lock(lock_) ({ \
    int ret = 0; \
    lock_.attempting_to_get = __FUNCTION__; \
    lock_.lock_name = #lock_; \
    if (!check_interrupts()) { \
        if (spinlock_check_and_lock(&lock_.lock_dat)) { \
            ret = 0; \
        } else { \
            ret = 1; \
            lock_.cpu_holding_lock = get_cpu_index(); \
            lock_.current_holder = __FUNCTION__; \
        } \
    } else { \
        spinlock_lock(&lock_.lock_dat); \
        lock_.cpu_holding_lock = get_cpu_index(); \
        lock_.current_holder = __FUNCTION__; \
    } \
    ret; \
})

#define interrupt_safe_unlock(lock_) \
    lock_.cpu_holding_lock = -1; \
    spinlock_unlock(&lock_.lock_dat);

#endif