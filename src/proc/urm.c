#include "urm.h"
#include "scheduler.h"
#include "exec_formats/elf.h"
#include "mm/vmm.h"
#include "mm/pmm.h"
#include "sys/smp.h"
#include "klibc/stdlib.h"
#include "klibc/errno.h"
#include "drivers/serial.h"
#include "drivers/pit.h"
#include "drivers/tty/tty.h"
#include <stddef.h>

urm_type_t urm_type;
void *urm_data;
lock_t urm_lock = {0, 0, 0, 0};
event_t urm_request_event = 0;

event_t *urm_done_event;
int *urm_return;

int urm_kill_thread(urm_kill_thread_data *data) {
    int64_t tid_to_kill = data->tid;
    kill_thread(tid_to_kill);
    urm_return = 0;
}

void urm_kill_process(urm_kill_process_data *data) {
    interrupt_safe_lock(sched_lock);
    process_t *process = processes[data->pid];

    uint64_t size = process->threads_size;
    int64_t *tids = kmalloc(sizeof(int64_t) * process->threads_size);
    for (uint64_t i = 0; i < process->threads_size; i++) {
        tids[i] = process->threads[i];
    }
    interrupt_safe_unlock(sched_lock);

    for (uint64_t i = 0; i < size; i++) {
        if (tids[i] != -1) {
            kill_thread(tids[i]);
        }
    }

    vmm_deconstruct_address_space((void *) process->cr3);
    clear_fds(data->pid);
    kfree(process->threads);
    delete_hashmap(process->ipc_handles);

    interrupt_safe_lock(sched_lock);
    kfree(processes[data->pid]);
    processes[data->pid] = (void *) 0;
    interrupt_safe_unlock(sched_lock);
    kfree(tids);
    urm_return = 0;
}

void urm_execve(urm_execve_data *data) {
    uint64_t starting_memory = pmm_get_used_mem();

    uint64_t entry_point = 0;
    auxv_auxc_group_t auxv_info;
    void *address_space = load_elf_addrspace(data->executable_path, &entry_point, 0, NULL, &auxv_info);
    if (!address_space) {
        sprintf("bruh momento [execve]\n");
        urm_return = ENOENT;
        return;
    }

    interrupt_safe_lock(sched_lock);
    process_t *current_process = processes[data->pid];
    for (uint64_t i = 0; i < current_process->threads_size; i++) {
        if (current_process->threads[i] != -1) {
            if (threads[current_process->threads[i]]) {
                kfree(threads[current_process->threads[i]]);
                threads[current_process->threads[i]] = (void *) 0;
            }
            current_process->threads[i] = -1;
            current_process->child_thread_count--;
        }
    }

    vmm_deconstruct_address_space((void *) current_process->cr3);

    current_process->current_brk = DEFAULT_BRK;
    current_process->cr3 = (uint64_t) address_space;

    thread_t *thread = create_thread(data->executable_path, (void *) entry_point, USER_STACK, 3);
    if (auxv_info.auxv) {
        thread->vars.auxc = auxv_info.auxc;
        thread->vars.auxv = auxv_info.auxv;
    }
    add_argv(&thread->vars, data->executable_path);

    thread->vars.envc = data->envc;
    thread->vars.argc = data->argc;
    thread->vars.enviroment = data->envp;
    thread->vars.argv = data->argv;

    interrupt_safe_unlock(sched_lock);

    add_new_child_thread(thread, data->pid);
    uint64_t ending_memory = pmm_get_used_mem();
    sprintf("Starting memory: %lx, Ending memory: %lx, Difference: %lx\n", starting_memory, ending_memory, ending_memory - starting_memory);
    urm_return = 0; // There is not code waiting for us, since we have replaced the thread
    urm_trigger_done = 0;
}

/* Epic */
void urm_thread() {
    while (1) {
        await_event(&urm_request_event); // Wait for a URM request
        switch (urm_type) {
            case URM_KILL_PROCESS:
                urm_kill_process(urm_data);
                break;
            case URM_KILL_THREAD:
                urm_kill_thread(urm_data);
                break;
            case URM_EXECVE:
                urm_execve(urm_data);
                break;
            default:
                break;
        }
        if (urm_trigger_done) {
            trigger_event(&urm_done_event);
        }
        unlock(urm_lock);
    }
}

int send_urm_request(void *data, urm_type_t type) {
    lock(urm_lock);
    urm_trigger_done = 1;
    urm_data = data;
    urm_type = type;
    trigger_event(&urm_request_event);
    await_event(&urm_done_event);
    return urm_return;
}

void send_urm_request_isr(void *data, urm_type_t type) {
    lock(urm_lock);
    urm_trigger_done = 0;
    urm_data = data;
    urm_type = type;
    trigger_event(&urm_request_event);
}

void send_urm_request_async(void *data, urm_type_t type) {
    lock(urm_lock);
    urm_trigger_done = 0;
    urm_data = data;
    urm_type = type;
    trigger_event(&urm_request_event);
}