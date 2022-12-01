#include "process_management.h"
#include "klibc/lock.h"
#include "proc/scheduler.h"

int64_t add_new_pid(process_t *process, int locked) {
    if (!locked) interrupt_safe_lock(sched_lock);

    int64_t new_pid = -1;
    for (uint64_t i = 0; i < process_list_size; i++) {
        if (!processes[i]) {
            new_pid = i;
            break;
        }
    }

    if (new_pid == -1) {
        processes = krealloc(processes, (process_list_size + 10) * sizeof(process_t *));
        new_pid = process_list_size;
        process_list_size += 10;
    }

    if (!locked) interrupt_safe_unlock(sched_lock);
    return new_pid;
}

int64_t add_new_tid(thread_t *thread, int locked) {
    if (!locked) interrupt_safe_lock(sched_lock);

    int64_t new_tid = -1;
    for (uint64_t i = 0; i < threads_list_size; i++) {
        if (!threads[i]) {
            new_tid = i;
            break;
        }
    }

    if (new_tid == -1) {
        threads = krealloc(threads, (threads_list_size + 10) * sizeof(thread_t *));
        new_tid = threads_list_size;
        threads_list_size += 10;
    }

    if (!locked) interrupt_safe_unlock(sched_lock);
    return new_tid;
}