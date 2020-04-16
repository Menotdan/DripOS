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

static void insert_to_queue(uint64_t ticks, int64_t tid) {
    interrupt_state_t state = interrupt_lock();
    lock(sleep_queue_lock);

    uint64_t total = 0;
    sleep_queue_t *cur = &base_queue;

    while (1) {
        // sprintf("\nloop 2");
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
    lock(sleep_queue_lock);
    lock_scheduler();
    sleep_queue_t *cur = base_queue.next;
    if (cur) {
        cur->time_left--;
        while (cur && cur->time_left == 0) {
            assert(cur);
            sleep_queue_t *next = cur->next;
            UNCHAIN_LINKED_LIST(cur);
            int64_t tid = cur->tid;
            kfree(cur);

            task_t *thread = get_thread_elem(tid);
            if (thread) { // In case the thread was killed in it's sleep
                assert(thread->state == SLEEP);
                thread->state = READY;
                log_debug("Set ready.");
                sprintf(" TID: %ld, CPU: %u", tid, get_cpu_locals()->cpu_index);
                thread->running = 0;
                log_debug("Turned off running.");
                sprintf(" TID: %ld, CPU: %u", tid, get_cpu_locals()->cpu_index);
            }
            unref_thread_elem(tid);

            cur = next;
        }
    }
    unlock_scheduler();
    unlock(sleep_queue_lock);
}

void sleep_ms(uint64_t ms) {
    interrupt_state_t state = interrupt_lock();
    log_debug("Inserting to queue");
    insert_to_queue(ms, get_cpu_locals()->current_thread->tid); // Insert to the thread sleep queue
    log_debug("Inserted to queue");
    lock_scheduler();
    sprintf("\nState: %u", (uint32_t) get_cpu_locals()->current_thread->state);
    assert(get_cpu_locals()->current_thread->state == RUNNING);
    get_cpu_locals()->current_thread->state = SLEEP;
    log_debug("Set sleep.");
    sprintf(" TID: %ld, CPU: %u", get_cpu_locals()->current_thread->tid, get_cpu_locals()->cpu_index);

    unlock_scheduler();
    assert(check_interrupts() == 0);
    lock(sleep_queue_lock); // Prevent the sleep queue from setting ready
    log_debug("Yielding");
    yield(); // Leave in case the scheduler hasn't scheduled us out itself
    log_debug("Yielded");
    unlock(sleep_queue_lock);
    interrupt_unlock(state);
}