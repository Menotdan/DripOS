#include "sleep_queue.h"
#include "drivers/pit.h"
#include "proc/scheduler.h"
#include "sys/smp.h"
#include "klibc/stdlib.h"
#include "klibc/lock.h"
#include "klibc/linked_list.h"

#include "drivers/serial.h"

lock_t sleep_queue_lock = {0, 0, 0};
sleep_queue_t base_queue = {0, 0, 0, 0};

void insert_to_queue(uint64_t ticks, int64_t tid) {
    interrupt_state_t state = interrupt_lock();
    lock(sleep_queue_lock);

    uint64_t total = 0;
    sleep_queue_t *cur = &base_queue;

    while (1) {
        sprintf("\nBoop");
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
    sleep_queue_t *new = kcalloc(sizeof(sleep_queue_t));
    CHAIN_LINKED_LIST(cur, new);
    new->time_left = before_relative;
    new->tid = tid;

    /* Subtract the queue so it remains relative */
    if (next) {
        next->time_left -= before_relative;
    }

    // sprintf("\nInserted.");
    unlock(sleep_queue_lock);
    interrupt_unlock(state);
}

void advance_time() {
    lock_scheduler();
    lock(sleep_queue_lock);
    sleep_queue_t *cur = base_queue.next;
    if (cur) {
        cur->time_left--;
        while (cur && cur->time_left == 0) {
            sprintf("\nBoop2");
            sleep_queue_t *next = cur->next;
            UNCHAIN_LINKED_LIST(cur);
            int64_t tid = cur->tid;
            kfree(cur);

            task_t *thread = get_thread_elem(tid);
            if (thread) { // In case the thread was killed in it's sleep
                sprintf("\nState: %lu", thread->state);
                //thread->state = READY;
            }

            cur = next;
        }
    }
    unlock(sleep_queue_lock);
    unlock_scheduler();
}

void sleep_ms(uint64_t ms) {
    interrupt_state_t state = interrupt_lock();
    lock_scheduler();
    sprintf("\nInserting to queue");
    insert_to_queue(ms, get_cpu_locals()->current_thread->tid); // Insert to the thread sleep queue
    sprintf("\nIn queue");
    //get_cpu_locals()->current_thread->state = SLEEP;
    sprintf("\nSleeping");
    unlock_scheduler();
    interrupt_unlock(state);

    yield();
}