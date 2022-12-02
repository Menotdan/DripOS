#include "urm.h"
#include "scheduler.h"
#include "exec_formats/elf.h"
#include "mm/vmm.h"
#include "mm/pmm.h"
#include "sys/smp.h"
#include "klibc/stdlib.h"
#include "klibc/errno.h"
#include "drivers/serial.h"
#include "drivers/tty/tty.h"
#include <stddef.h>

urm_type_t urm_type;
void *urm_data;
lock_t urm_lock = {0, 0, 0, 0};
event_t urm_request_event = 0;
event_t urm_done_event = 0;
int urm_trigger_done = 1;
int urm_return = 0;

void kill_thread(int64_t tid) {
    char thread_name[50] = "";
    if (threads[tid]) {
        strcat(thread_name, threads[tid]->name);
    }

    kprintf("kill_thread(%ld) - %s (internal)\n", tid, thread_name);
    sprintf("tid %ld - %s\n", tid, thread_name);
    interrupt_safe_lock(sched_lock);
    thread_t *thread = threads[tid];
    assert(thread);
    thread->state = BLOCKED;

    if (thread->cpu != -1) {
        uint8_t cpu = (uint8_t) thread->cpu;
        interrupt_safe_unlock(sched_lock);
        send_ipi(cpu, (1 << 14) | 253); // Reschedule, in case the cpu is still running the thread
        while (thread->cpu != -1) { asm("pause"); }
        interrupt_safe_lock(sched_lock);
    }

    /* TODO: do proper cleanup */
    if (!threads[tid]) {
        //sprintf("thread is null\n");
    }

    sprintf("Magic KILLTID: %ld\n", tid);
    threads[tid] = (void *) 0;
    kfree(thread);

    for (uint64_t i = 0; i < threads_list_size; i++) {
        sprintf("Remaining thread: %lx %d\n", threads[i], i);
    }

    interrupt_safe_unlock(sched_lock);
}

void urm_kill_thread(urm_kill_thread_data *data) {
    int64_t tid_to_kill = data->tid;
    kill_thread(tid_to_kill);
    urm_return = 0;
}

void urm_kill_process(urm_kill_process_data *data) {
    sprintf("Remaining kill_process.\n");
    interrupt_safe_lock(sched_lock);
    process_t *process = processes[data->pid];

    uint64_t size = process->threads_size;
    int64_t *tids = kmalloc(sizeof(int64_t) * process->threads_size);
    sprintf("Remaining tids in process: %lu\n", process->threads_size);
    for (uint64_t i = 0; i < process->threads_size; i++) {
        tids[i] = process->threads[i];
    }
    interrupt_safe_unlock(sched_lock);

    for (uint64_t i = 0; i < size; i++) {
        sprintf("TIDs: %ld\n", tids[i]);
        if (tids[i] != -1) {
            sprintf("urm_kill ?????????????????\n");
            sprintf("BYPASSED BYPASSED WHOOT WHOOT %ld\n", tids[i]);
            kprintf("kill_thread(%ld)\n", tids[i]);
            sprintf("tid %ld - %s\n", tids[i], threads[tids[i]]->name);
            kill_thread(tids[i]);
        }
    }

    interrupt_safe_lock(sched_lock);
    kfree(processes[data->pid]);
    processes[data->pid] = (void *) 0;
    interrupt_safe_unlock(sched_lock);
    urm_return = 0;
}

void urm_execve(urm_execve_data *data) {
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

    thread->vars.envc = data->envc;
    thread->vars.argc = data->argc;
    thread->vars.enviroment = data->envp;
    thread->vars.argv = data->argv;

    interrupt_safe_unlock(sched_lock);

    sprintf("Magic execve, name thread: %s, name proc: %s, addr thread: %lx, addr proc: %lx, entrypoint: %lx\n", thread->name, current_process->name, thread, current_process, entry_point);

    add_new_child_thread(thread, data->pid);
    sprintf("Remaining code is add_new, tid: %ld, pid: %ld\n", thread->tid, data->pid);
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