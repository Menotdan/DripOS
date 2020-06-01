#include "sleep_queue.h"
#include "drivers/pit.h"
#include "proc/scheduler.h"
#include "sys/smp.h"
#include "mm/vmm.h"
#include "klibc/stdlib.h"
#include "klibc/lock.h"
#include "klibc/linked_list.h"
#include "klibc/errno.h"

#include "drivers/serial.h"

lock_t sleep_queue_lock = {0, 0, 0, 0};
sleep_queue_t base_queue = {0, 0, 0, 0};
uint64_t lagged_ticks = 0;

static void insert_to_queue(uint64_t ticks, int64_t tid) {
    lock(sleep_queue_lock);

    uint64_t total = 0;
    sleep_queue_t *cur = &base_queue;

    while (1) {
        total += cur->time_left;

        if (!cur->next) {
            break;
        }
        if (total <= ticks) {
            if (cur->next) {
                if (total + cur->next->time_left >= ticks) {
                    break;
                }
            } else {
                break;
            }
        }
        cur = cur->next;
    }

    sleep_queue_t *next = cur->next;
    uint64_t before_relative = ticks - total;
    sleep_queue_t *new = get_cpu_locals()->current_thread->sleep_node;
    new->next = (void *) 0;
    new->prev = (void *) 0;
    CHAIN_LINKED_LIST(cur, new);
    new->time_left = before_relative;
    new->tid = tid;

    /* Subtract the queue so it remains relative */
    if (next) {
        next->time_left -= before_relative;
    }
    unlock(sleep_queue_lock);
}

void advance_time() {
    if (!interrupt_safe_lock(sched_lock)) {
        lagged_ticks += 1;
        return;
    }

    lock(sleep_queue_lock);
    if (lagged_ticks > (sched_period * 3)) {
        sprintf("[DripOS] Warning, kernel may be lagging, %lu ticks skipped.", lagged_ticks);
    }
    for (uint64_t i = 0; i < lagged_ticks + 1; i++) { // Count down for all the lagged ticks + the current tick
        sleep_queue_t *cur = base_queue.next;
        if (cur) {
            cur->time_left--;
            while (cur && cur->time_left == 0) {
                assert(cur);
                sleep_queue_t *next = cur->next;
                UNCHAIN_LINKED_LIST(cur);
                int64_t tid = cur->tid;

                thread_t *thread = get_thread_elem(tid);
                if (thread) { // In case the thread was killed in it's sleep
                    assert(thread->state == SLEEP);
                    thread->state = READY;
                    thread->running = 0;
                }
                unref_thread_elem(tid);

                cur = next;
            }
        }
    }
    lagged_ticks = 0;
    unlock(sleep_queue_lock);
    interrupt_safe_unlock(sched_lock);
}

void sleep_ms(uint64_t ms) {
    interrupt_safe_lock(sched_lock);
    assert(get_cpu_locals()->current_thread->state == RUNNING);
    get_cpu_locals()->current_thread->state = SLEEP;

    insert_to_queue(ms, get_cpu_locals()->current_thread->tid); // Insert to the thread sleep queue
    force_unlocked_schedule(); // Leave in case the scheduler hasn't scheduled us out itself
}

/* Nanosleep syscall */
int nanosleep(struct timespec *req, struct timespec *rem) {
    if (!range_mapped(req, sizeof(struct timespec))) {
        get_thread_locals()->errno = -EFAULT;
        return 1;
    }
    
    if (!range_mapped(rem, sizeof(struct timespec)) && rem) {
        get_thread_locals()->errno = -EFAULT;
        return 1;
    }

    if (req->nanoseconds > 999999999) {
        get_thread_locals()->errno = -EINVAL;
        return 1;
    }
    sleep_ms((req->nanoseconds / 1000000) + (req->seconds * 1000));

    return 0;
}