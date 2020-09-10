#include "event.h"
#include "proc/scheduler.h"
#include "sys/smp.h"
#include "klibc/lock.h"
#include "drivers/pit.h"

void await_event(event_t *e) {
    if (*e) {
        atomic_dec((uint32_t *) e);
        return;
    }

    interrupt_safe_lock(sched_lock);
    thread_t *current_thread = get_cpu_locals()->current_thread;
    current_thread->event = e;
    current_thread->state = WAIT_EVENT;
    force_unlocked_schedule();
}

void await_event_timeout(event_t *e, uint64_t timeout) {
    interrupt_safe_lock(sched_lock);
    thread_t *current_thread = get_cpu_locals()->current_thread;
    current_thread->event = e;
    current_thread->state = WAIT_EVENT_TIMEOUT;
    current_thread->event_timeout = timeout;
    current_thread->event_wait_start = global_ticks;
    force_unlocked_schedule();
}

void trigger_event(event_t *e) {
    atomic_inc((uint32_t *) e);
}