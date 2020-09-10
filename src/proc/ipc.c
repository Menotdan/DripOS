#include "ipc.h"
#include "event.h"
#include "scheduler.h"
#include "sys/smp.h"
#include "klibc/stdlib.h"
#include "drivers/pit.h"

/* IPC server init includes */
#include "drivers/vesa.h"

int register_ipc_handle(int port) {
    interrupt_safe_lock(sched_lock);
    process_t *cur_process = processes[get_cpu_locals()->current_thread->parent_pid];
    interrupt_safe_unlock(sched_lock);

    lock(cur_process->ipc_create_handle_lock);
    ipc_handle_t *handle = hashmap_get_elem(cur_process->ipc_handles, port);
    if (handle) {
        return 1;
    }

    handle = kcalloc(sizeof(ipc_handle_t));
    hashmap_set_elem(cur_process->ipc_handles, port, handle);
    unlock(cur_process->ipc_create_handle_lock);

    return 0;
}

ipc_handle_t *wait_ipc(int port) {
    interrupt_safe_lock(sched_lock);
    process_t *cur_process = processes[get_cpu_locals()->current_thread->parent_pid];
    interrupt_safe_unlock(sched_lock);

    ipc_handle_t *handle = hashmap_get_elem(cur_process->ipc_handles, port);
    if (!handle) {
        return (void *) 0;
    }

    handle->listening = 1;

    event_t ipc_await_event = 0;
    handle->ipc_event = &ipc_await_event;
    await_event(&ipc_await_event);
    handle->listening = 0;
    return handle;
}

union ipc_err write_ipc_server(int pid, int port, void *buf, int size) {
    interrupt_safe_lock(sched_lock);
    process_t *target_process = processes[pid];
    interrupt_safe_unlock(sched_lock);

    lock(target_process->ipc_create_handle_lock);
    ipc_handle_t *handle = hashmap_get_elem(target_process->ipc_handles, port);
    unlock(target_process->ipc_create_handle_lock);
    if (!handle) {
        union ipc_err err;
        err.parts.err = IPC_NOT_LISTENING;
        return err;
    }

    if (!handle->listening) {
        uint64_t start_tick = global_ticks;
        uint64_t listening = 1;
        while (!handle->listening) {
            if (start_tick + IPC_CONNECT_TIMEOUT_MS > global_ticks) {
                listening = 0;
                break;
            }
        }

        if (!listening) {
            union ipc_err err;
            err.parts.err = IPC_NOT_LISTENING;
            return err; // Server hasn't setup yet
        }
    }

    if (spinlock_check_and_lock(&handle->connect_lock.lock_dat)) { // Was locked
        uint64_t start_ticks = global_ticks;
        uint8_t got_lock = 0;
        while (start_ticks + IPC_CONNECT_TIMEOUT_MS > global_ticks) {
            if (!spinlock_check_and_lock(&handle->connect_lock.lock_dat)) {
                got_lock = 1;
                break; // We own the lock now
            }
        }

        if (!got_lock) {
            union ipc_err err;
            err.parts.err = IPC_CONNECTION_TIMEOUT;
            return err;
        }
    }

    // If we made it here, we have the connection lock

    event_t wait_server_done = 0;

    /* Set data, trigger event, wait */
    handle->pid = get_cpu_locals()->current_thread->parent_pid;
    handle->ipc_completed = &wait_server_done;
    handle->buffer = buf;
    handle->size = size;
    handle->operation_type = IPC_OPERATION_WRITE;
    trigger_event(handle->ipc_event);
    await_event(handle->ipc_completed);
    atomic_dec(&handle->connect_lock.lock_dat);

    if (!handle->err) {
        union ipc_err err;
        err.parts.err = IPC_SUCCESS;
        return err;
    } else {
        union ipc_err err;
        err.parts.err = handle->err;
        err.parts.size = handle->size;
        return err;
    }
}

union ipc_err read_ipc_server(int pid, int port, void *buf, int size) {
    interrupt_safe_lock(sched_lock);
    process_t *target_process = processes[pid];
    interrupt_safe_unlock(sched_lock);

    lock(target_process->ipc_create_handle_lock);
    ipc_handle_t *handle = hashmap_get_elem(target_process->ipc_handles, port);
    unlock(target_process->ipc_create_handle_lock);
    if (!handle) {
        union ipc_err err;
        err.parts.err = IPC_NOT_LISTENING;
        return err;
    }

    if (!handle->listening) {
        uint64_t start_tick = global_ticks;
        uint64_t listening = 1;
        while (!handle->listening) {
            if (start_tick + IPC_CONNECT_TIMEOUT_MS <= global_ticks) {
                listening = 0;
                break;
            }
        }

        if (!listening) {
            union ipc_err err;
            err.parts.err = IPC_NOT_LISTENING;
            return err; // Server hasn't setup yet
        }
    }

    if (spinlock_check_and_lock(&handle->connect_lock.lock_dat)) { // Was locked
        uint64_t start_ticks = global_ticks;
        uint8_t got_lock = 0;
        while (start_ticks + IPC_CONNECT_TIMEOUT_MS > global_ticks) {
            if (!spinlock_check_and_lock(&handle->connect_lock.lock_dat)) {
                got_lock = 1;
                break; // We own the lock now
            }
        }

        if (!got_lock) {
            union ipc_err err;
            err.parts.err = IPC_CONNECTION_TIMEOUT;
            return err;
        }
    }

    // If we made it here, we have the connection lock

    event_t wait_server_done = 0;

    /* Set data, trigger event, wait */
    handle->pid = get_cpu_locals()->current_thread->parent_pid;
    handle->ipc_completed = &wait_server_done;
    handle->buffer = buf;
    handle->size = size;
    handle->operation_type = IPC_OPERATION_READ;
    trigger_event(handle->ipc_event);
    await_event(handle->ipc_completed);
    atomic_dec(&handle->connect_lock.lock_dat);

    if (!handle->err) {
        union ipc_err err;
        err.parts.err = IPC_SUCCESS;
        return err;
    } else {
        union ipc_err err;
        err.parts.err = handle->err;
        err.parts.size = handle->size;
        return err;
    }
}

void setup_ipc_servers() {
    /* VESA IPC server */
    thread_t *vesa_ipc = create_thread("VESA IPC server", vesa_ipc_server, (uint64_t) kcalloc(TASK_STACK_SIZE) + TASK_STACK_SIZE, 0);
    add_new_child_thread(vesa_ipc, 0);
}