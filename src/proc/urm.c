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
    
    return 0;
}

int urm_kill_process(urm_kill_process_data *data) {
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

    if (process->cr3 != base_kernel_cr3) {
        vmm_deconstruct_address_space((void *) process->cr3);
    }
    clear_fds(data->pid);
    kfree(process->threads);
    delete_hashmap(process->ipc_handles);

    interrupt_safe_lock(sched_lock);
    kfree(processes[data->pid]);
    processes[data->pid] = (void *) 0;
    interrupt_safe_unlock(sched_lock);
    kfree(tids);
    return 0;
}

int urm_execve(urm_execve_data *data) {
    uint64_t entry_point = 0;
    auxv_auxc_group_t auxv_info;
    void *address_space = load_elf_addrspace(data->executable_path, &entry_point, 0, NULL, &auxv_info);
    if (!address_space) {
        sprintf("bruh momento [execve]\n");
        return ENOENT;
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

    return 0; // There is not code waiting for us, since we have replaced the thread
}

void urm_runner(void *data, urm_runtime_params_t *params) {
    int ret_temp = 0;
    if (!params->return_val) {
        params->return_val = &ret_temp;
    }

    switch (params->type) {
        case URM_KILL_PROCESS:
            *params->return_val = urm_kill_process(data);
            break;
        case URM_KILL_THREAD:
            *params->return_val = urm_kill_thread(data);
            break;
        case URM_EXECVE:
            *params->return_val = urm_execve(data);
            break;
        default:
            break;
    }

    if (params->done_event) {
        trigger_event(params->done_event);
    }
    kfree(data);

    urm_kill_process_data *self_kill_data = kcalloc(sizeof(urm_kill_process_data));
    self_kill_data->pid = get_cur_pid();
    send_urm_request_async(self_kill_data, URM_KILL_RUNNER);

    interrupt_safe_lock(sched_lock);
    get_cur_thread()->state = BLOCKED;
    force_unlocked_schedule();
}

/* Epic */
void urm_thread() {
    while (1) {
        await_event(&urm_request_event); // Wait for a URM request

        int param_size = 0;
        switch (urm_type) {
            case URM_KILL_PROCESS:
                param_size = sizeof(urm_kill_process_data);               
                break;
            case URM_KILL_THREAD:
                param_size = sizeof(urm_kill_thread_data);
                break;
            case URM_EXECVE:
                param_size = sizeof(urm_execve_data);
                break;
            case URM_KILL_RUNNER:
                urm_kill_process(urm_data);
                break;
            default:
                break;
        }

        if (param_size == 0) {
            // Reset state
            urm_done_event = NULL;
            urm_data = NULL;
            urm_return = NULL;

            unlock(urm_lock);
            continue; // Skip, runner requesting kill or bad request.
        }

        void *urm_data_copy = kcalloc(param_size);
        memcpy(urm_data, urm_data_copy, param_size);

        urm_runtime_params_t *runtime_params = kcalloc(sizeof(runtime_params));
        runtime_params->done_event = urm_done_event;
        runtime_params->return_val = urm_return;
        runtime_params->type = urm_type;

        thread_t *new_urm_thread = add_basic_kernel_thread("URM Worker", urm_runner, BLOCKED);
        new_urm_thread->regs.rdi = (uint64_t) urm_data_copy;
        new_urm_thread->regs.rsi = (uint64_t) runtime_params;

        interrupt_safe_lock(sched_lock);
        new_urm_thread->state = READY;
        interrupt_safe_unlock(sched_lock);

        // Reset state
        urm_done_event = NULL;
        urm_data = NULL;
        urm_return = NULL;

        unlock(urm_lock);
    }
}

int send_urm_request(void *data, urm_type_t type) {
    lock(urm_lock);
    urm_data = data;
    urm_type = type;
    trigger_event(&urm_request_event);

    event_t *event_to_wait = kcalloc(sizeof(event_t));
    int *return_value = kcalloc(sizeof(int));

    urm_done_event = event_to_wait;
    urm_return = return_value;
    await_event(event_to_wait);

    int return_value_storage = *return_value;
    kfree(return_value);
    kfree(event_to_wait);

    return return_value_storage;
}

void send_urm_request_isr(void *data, urm_type_t type) {
    lock(urm_lock);
    urm_data = data;
    urm_type = type;
    trigger_event(&urm_request_event);
}

void send_urm_request_async(void *data, urm_type_t type) {
    lock(urm_lock);
    urm_data = data;
    urm_type = type;
    trigger_event(&urm_request_event);
}