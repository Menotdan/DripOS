#include "scheduler.h"
#include "mxcsr.h"
#include "x87_control.h"
#include "safe_userspace.h"
#include "klibc/stdlib.h"
#include "klibc/string.h"
#include "klibc/debug.h"
#include "klibc/queue.h"
#include "klibc/errno.h"
#include "klibc/logger.h"
#include "drivers/tty/tty.h"
#include "drivers/serial.h"
#include "io/msr.h"
#include "mm/vmm.h"
#include "mm/pmm.h"
#include "sys/smp.h"
#include "sys/apic.h"
#include <stddef.h>

#include "drivers/pit.h"
#include "urm.h"

extern char syscall_stub[];

uint64_t threads_list_size = 0;
uint64_t process_list_size = 0;
thread_t **threads;
process_t **processes;

uint64_t process_count = 0;

uint8_t scheduler_enabled = 0;
interrupt_safe_lock_t sched_lock = {0, 0, 0, 0, -1};

task_regs_t default_kernel_regs = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0x10,0x8,0,0x202,0,0x1F80,0x33f};
task_regs_t default_user_regs = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0x23,0x1B,0,0x202,0,0x1F80,0x33f};

int scheduler_ran = 0;

uint64_t get_thread_list_size() {
    return threads_list_size;
}

void lock_scheduler() {
    interrupt_safe_lock(sched_lock);
}

void unlock_scheduler() {
    interrupt_safe_unlock(sched_lock);
}

void send_scheduler_ipis() {
    madt_ent0_t **cpus = (madt_ent0_t **) vector_items(&cpu_vector);
    for (uint64_t i = 0; i < cpu_vector.items_count; i++) {
        if (i < cpu_vector.items_count) {
            if ((cpus[i]->cpu_flags & 1 || cpus[i]->cpu_flags & 2) && cpus[i]->apic_id != get_lapic_id()) {
                send_ipi(cpus[i]->apic_id, (1 << 14) | 253); // Send interrupt 253
                while (!scheduler_ran) { asm("pause"); }
                scheduler_ran = 0;
            }
        }
    }
}

void _idle() {
    while (1) {
        asm volatile("hlt");
    }
}

char default_sse_state[512] __attribute__((aligned(16)));

void init_scheduler_msr() {
    write_msr(0xC0000081, read_msr(0xC0000081) | ((uint64_t) 0x8 << 32));
    write_msr(0xC0000081, read_msr(0xC0000081) | ((uint64_t) 0x18 << 48));
    write_msr(0xC0000082, (uint64_t) syscall_stub); // Start execution at the syscall stub when a syscall occurs
    write_msr(0xC0000084, 0); // Mask nothing
    write_msr(0xC0000080, read_msr(0xC0000080) | 1); // Set the syscall enable bit
}

void init_sched_cpu_locals() {
    uint64_t idle_rsp = (uint64_t) kcalloc(0x1000) + 0x1000;
    thread_t *new_idle = create_thread("Idle thread", _idle, idle_rsp, 0);
    new_idle->state = BLOCKED; // Idle thread will *not* run unless provoked
    int64_t idle_tid = start_thread(new_idle);

    get_cpu_locals()->idle_tid = idle_tid;
    sprintf("Idle task: %ld\n", get_cpu_locals()->idle_tid);
    get_cpu_locals()->idle_start_tsc = read_tsc();
    get_cpu_locals()->currently_idle = 1;
    get_cpu_locals()->total_tsc = read_tsc();
}

/* Initialize the BSP for scheduling */
void scheduler_init_bsp() {
    init_scheduler_msr();
    asm volatile("fxsave %0;"::"m"(default_sse_state));

    init_sched_cpu_locals(); // Sets up CPU locals for the scheduler including the idle task
}

/* Initilialize an AP for scheduling */
void scheduler_init_ap() {
    init_scheduler_msr();

    init_sched_cpu_locals(); // Sets up CPU locals for the scheduler including the idle task
}

/* Create a new thread *and* add it to the list */
int64_t new_thread(char *name, void (*main)(), uint64_t rsp, int64_t pid, uint8_t ring) {
    thread_t *new_task = create_thread(name, main, rsp, ring);
    int64_t new_tid = add_new_child_thread(new_task, pid);
    return new_tid;
}

/* Add the thread to the list */
int64_t start_thread(thread_t *thread) {
    interrupt_safe_lock(sched_lock);
    int64_t tid = add_new_tid(1);
    thread->tid = tid;

    threads[tid] = thread;

    interrupt_safe_unlock(sched_lock);
    return tid;
}

/* Allocate data for a new thread data block and return it */
thread_t *create_thread(char *name, void (*main)(), uint64_t rsp, uint8_t ring) {
    /* Allocate new task and it's kernel stack */
    thread_t *new_task = kcalloc(sizeof(thread_t));
    new_task->kernel_stack = (uint64_t) kcalloc(0x1000) + 0x1000;

    /* Setup ring */
    if (ring == 3) {
        new_task->regs = default_user_regs;
    } else {
        new_task->regs = default_kernel_regs;
    }
    /* Set rip, rsp, cr3, and name */
    new_task->regs.rip = (uint64_t) main;
    new_task->regs.rsp = rsp;
    new_task->ring = ring;
    new_task->regs.cr3 = base_kernel_cr3;
    new_task->sleep_node = kcalloc(sizeof(sleep_queue_t));
    new_task->state = READY;
    strcpy(name, new_task->name);
    memcpy((uint8_t *) default_sse_state, (uint8_t *) new_task->sse_region, 512);

    /* Create null argv, enviroment, and auxv */
    new_task->vars.envc = 0;
    new_task->vars.argc = 0;
    new_task->vars.auxc = 0;
    new_task->vars.enviroment = NULL;
    new_task->vars.auxv = NULL;
    new_task->vars.argv = NULL;
    new_task->parent = NULL;

    return new_task;
}

/* Expects something in higher half */
void add_argv(main_thread_vars_t *vars, char *string) {
    vars->argv = krealloc(vars->argv, (vars->argc + 1) * sizeof(char *));
    vars->argv[vars->argc] = string;
    vars->argc++;
}

void add_auxv(main_thread_vars_t *vars, auxv_t *auxv) {
    vars->auxv = krealloc(vars->auxv, (vars->auxc + 2) * sizeof(auxv_t));
    vars->auxv[vars->auxc + 1].a_type = auxv->a_type;
    vars->auxv[vars->auxc + 1].a_un.a_val = auxv->a_un.a_val;
    vars->auxc++;
}

/* Expects something in higher half */
void add_env(main_thread_vars_t *vars, char *string) {
    vars->enviroment = krealloc(vars->enviroment, (vars->envc + 2) * sizeof(char *));
    vars->enviroment[vars->envc + 1] = string;
    vars->envc++;
}

void push_thread_stack(uint64_t *stack_addr, uint64_t value) {
    *stack_addr -= 8;
    uint64_t *stack = (void *) *stack_addr;
    *stack = value;
}

int64_t add_new_child_thread_no_stack_init(thread_t *thread, int64_t pid) {
    interrupt_safe_lock(sched_lock);

    /* Find the parent process */
    process_t *new_parent = (void *) 0;
    if ((uint64_t) pid < process_list_size) {
        new_parent = processes[pid];
    }
    if (!new_parent) { sprintf("[Scheduler] Couldn't find parent\n"); return -1; }

    /* Store the new task and save its TID */

    int64_t new_tid = add_new_tid(1);
    threads[new_tid] = thread;

    thread->tid = new_tid;
    thread->regs.cr3 = new_parent->cr3; // Inherit parent's cr3
    thread->parent_pid = pid;
    thread->parent = new_parent;

    /* Add the TID to it's parent's threads list */
    int64_t index = add_to_process(new_parent, 1);

    new_parent->threads[index] = thread->tid;

    interrupt_safe_unlock(sched_lock);
    return new_tid;
}

/* Add a new thread to the dynarray and as a child of a process */
int64_t add_new_child_thread(thread_t *thread, int64_t pid) {
    interrupt_safe_lock(sched_lock);

    /* Find the parent process */
    process_t *new_parent = (void *) 0;
    if ((uint64_t) pid < process_list_size) {
        new_parent = processes[pid];
    }
    if (!new_parent) { sprintf("[Scheduler] Couldn't find parent\n"); return -1; }

    /* Store the new task and save its TID */

    int64_t new_tid = add_new_tid(1);
    threads[new_tid] = thread;

    thread->tid = new_tid;
    thread->regs.cr3 = new_parent->cr3; // Inherit parent's cr3
    thread->parent_pid = pid;
    thread->parent = new_parent;

    if (new_parent->child_thread_count == 0) { // No children
        // -1 because kmalloc creates boundaries so page might not be mapped :|
        void *phys = virt_to_phys((void *) (thread->regs.rsp - 1), (page_table_t *) thread->regs.cr3);
        if ((uint64_t) phys == 0xFFFFFFFFFFFFFFFF) {
            sprintf("RSP not mapped :thonk:\n");
            goto done;
        }

        uint64_t stack = GET_HIGHER_HALF(uint64_t, phys) + 1;
        uint64_t old_stack = stack;

        int argc = thread->vars.argc;
        char **argv = thread->vars.argv;

        /* Add argc, argv, and auxv */

        // Actual data
        uint64_t *string_offsets = kcalloc(sizeof(uint64_t) * argc + sizeof(uint64_t) * thread->vars.envc);
        uint64_t total_string_len = 0;
        //sprintf("argc: %d\n", argc);
        for (int i = 0; i < argc; i++) {
            //sprintf("i = %d with argv[i] = %lx\n", i, argv[i]);

            total_string_len += (strlen(argv[i]) + 1);
            string_offsets[i] = total_string_len;

            char *new_string = (char *) (old_stack - string_offsets[i]);
            //sprintf("String Data: %s, String: %lx, string offset: %lu\n", argv[i], new_string, string_offsets[i]);
            strcpy(argv[i], new_string);
        }
        asm("hlt");
        for (int i = argc; i < argc + thread->vars.envc; i++) {
            //sprintf("i = %d with envp[i] = %lx\n", i, thread->vars.enviroment[i - argc]);
            total_string_len += (strlen(thread->vars.enviroment[i - argc]) + 1);
            string_offsets[i] = total_string_len;

            char *new_string = (char *) (old_stack - string_offsets[i]);
            //sprintf("String Data: %s, String: %lx, string offset: %lu\n", thread->vars.enviroment[i - argc], new_string, string_offsets[i]);
            strcpy(thread->vars.enviroment[i - argc], new_string);
        }
        stack -= total_string_len;
        stack = stack & ~(0b1111); // align the stack

        //                       argc sz    auxv sz                  enviroment pointers     extra nulls
        uint64_t size_of_items = argc * 8 + thread->vars.auxc * 16 + thread->vars.envc * 8 + 16;
        push_thread_stack(&stack, 0);
        if (size_of_items & 15) {
            push_thread_stack(&stack, 0); // realign the stack
        }
        uint64_t auxv_count = thread->vars.auxc;
        if (auxv_count) {
            for (uint64_t i = auxv_count - 1;; i--) {
                push_thread_stack(&stack, (uint64_t) thread->vars.auxv[i].a_un.a_ptr);
                push_thread_stack(&stack, (uint64_t) thread->vars.auxv[i].a_type);
                if (i == 0) {
                    break;
                }
            }
        }
        uint64_t auxv_offset = old_stack - stack;

        push_thread_stack(&stack, 0);
        uint64_t enviroment_count = thread->vars.envc;
        if (enviroment_count) {
            for (uint64_t i = enviroment_count - 1;; i--) {
                push_thread_stack(&stack, thread->regs.rsp - string_offsets[i + argc]);
                if (i == 0) {
                    break;
                }
            }
        }

        push_thread_stack(&stack, 0);
        if (argc) {
            for (uint64_t i = argc - 1;; i--) {
                push_thread_stack(&stack, thread->regs.rsp - string_offsets[i]);
                if (i == 0) {
                    break;
                }
            }
        }
        uint64_t argv_offset = old_stack - stack;

        push_thread_stack(&stack, argc);


        thread->regs.rdi = (uint64_t) argc;
        thread->regs.rsi = thread->regs.rsp - argv_offset;
        thread->regs.rdx = thread->regs.rsp - auxv_offset;
        thread->regs.rsp -= (old_stack - stack);

        assert(!(stack & 15));
    }

    int64_t index = -1;
done:
    /* Add the TID to it's parent's threads list */
    index = add_to_process(new_parent, 1);

    new_parent->threads[index] = thread->tid;

    interrupt_safe_unlock(sched_lock);
    return new_tid;
}

process_t *create_process(char *name, void *new_cr3) {
    /* Allocate new process */
    process_t *new_process = kcalloc(sizeof(process_t));

    /* New cr3 */
    new_process->cr3 = (uint64_t) new_cr3;

    /* Default UID and GID */
    new_process->uid = 0;
    new_process->gid = 0;
    new_process->fd_table = kcalloc(10 * sizeof(fd_entry_t));
    new_process->fd_table_size = 10;
    new_process->current_brk = DEFAULT_BRK;
    new_process->ipc_handles = init_hashmap();

    new_process->local_watchpoint1_active = 0;
    new_process->local_watchpoint2_active = 0;
    new_process->local_watchpoint1 = 0;
    new_process->local_watchpoint2 = 0;

    strcpy(name, new_process->name);

    return new_process;
}


/* Add a process to the process list */
int64_t add_process(process_t *process) {
    interrupt_safe_lock(sched_lock);

    /* Add element to the list and save the PID */
    int64_t pid = add_new_pid(1);
    processes[pid] = process;
    processes[pid]->pid = pid;

    interrupt_safe_unlock(sched_lock);

    /* Open stdout, stdin, and stderr */
    open_remote_fd("/dev/tty1", 0, pid); // stdout
    open_remote_fd("/dev/tty1", 0, pid); // stdin
    open_remote_fd("/dev/tty1", 0, pid); // stderr

    process_count++;
    return pid;
}

/* Allocate new data for a process, then return the new PID */
int64_t new_process(char *name, void *new_cr3) {
    process_t *process = create_process(name, new_cr3);
    int64_t pid = add_process(process);
    return pid;
}

/* Wrapper for some other parts of the "API" */
void new_kernel_process(char *name, void (*main)()) {
    int64_t task_parent_pid = new_process(name, (void *) base_kernel_cr3);
    uint64_t new_rsp = (uint64_t) kcalloc(TASK_STACK_SIZE) + TASK_STACK_SIZE;

    new_thread(name, main, new_rsp, task_parent_pid, 0);
}

thread_t *add_basic_kernel_thread(char *name, void (*main)(), uint8_t init_state) {
    int64_t thread_parent_pid = new_process(name, (void *) base_kernel_cr3);
    uint64_t new_rsp = (uint64_t) kcalloc(TASK_STACK_SIZE) + TASK_STACK_SIZE;

    thread_t *added_thread = create_thread(name, main, new_rsp, 0);
    added_thread->state = init_state;
    add_new_child_thread_no_stack_init(added_thread, thread_parent_pid);

    return added_thread;
}

void kill_thread(int64_t tid) {
    char thread_name[50] = "";
    if (threads[tid]) {
        strcat(thread_name, threads[tid]->name);
    }

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

    if (thread->parent) {
        thread->parent->child_thread_count--;
    }

    /* TODO: do proper cleanup  (Deconstruct address space, etc) */
    threads[tid] = (void *) 0;
    kfree(thread);

    interrupt_safe_unlock(sched_lock);
}

int64_t pick_task() {
    int64_t tid_ret = -1;

    if (!(get_cur_thread())) {
        return tid_ret;
    }

    /* Prioritze tasks right after the current task */
    for (int64_t t = get_cur_thread()->tid + 1; (uint64_t) t < threads_list_size; t++) {
        thread_t *task = threads[t];
        if (task) {
            if (task->state == READY || task->state == WAIT_EVENT || task->state == WAIT_EVENT_TIMEOUT) {
                tid_ret = task->tid;
                return tid_ret;
            }
        }
    }

    /* Then look at the rest of the tasks */
    for (int64_t t = 0; t < get_cur_thread()->tid + 1; t++) {
        thread_t *task = threads[t];
        if (task) {
            if (task->state == READY || task->state == WAIT_EVENT || task->state == WAIT_EVENT_TIMEOUT) {
                tid_ret = task->tid;
                return tid_ret;
            }
        }
    }

    return tid_ret; // Return -1 by default for idle
}

void yield() {
    if (scheduler_enabled) {
        interrupt_safe_lock(sched_lock);
        asm volatile("int $254");
    }
}

void force_unlocked_schedule() {
    if (scheduler_enabled) {
        asm volatile("int $254");
    }
}

void schedule_runner(int_reg_t *r) {
    if (!spinlock_check_and_lock(&sched_lock.lock_dat)) {
        sched_lock.current_holder = __FUNCTION__;
        schedule(r);
    } else {
        if (get_cpu_locals()->currently_idle) {
            get_cpu_locals()->idle_tsc_count += read_tsc() - get_cpu_locals()->idle_start_tsc;
            get_cpu_locals()->idle_start_tsc = read_tsc();
        }  else {
            get_cpu_locals()->active_tsc_count += read_tsc() - get_cpu_locals()->active_start_tsc;
            get_cpu_locals()->active_start_tsc = read_tsc();
        }
    }
}

void schedule_bsp(int_reg_t *r) {
    send_scheduler_ipis();
    schedule_runner(r);
}

void schedule_ap(int_reg_t *r) {
    schedule_runner(r);
    scheduler_ran = 1;
}

void schedule(int_reg_t *r) {
    int used_to_be_idle = 0;
    int used_to_be_active = 0;

    thread_t *running_task = get_cur_thread();
    if (running_task) {
        running_task->regs.rax = r->rax;
        running_task->regs.rbx = r->rbx;
        running_task->regs.rcx = r->rcx;
        running_task->regs.rdx = r->rdx;
        running_task->regs.rbp = r->rbp;
        running_task->regs.rdi = r->rdi;
        running_task->regs.rsi = r->rsi;
        running_task->regs.r8 = r->r8;
        running_task->regs.r9 = r->r9;
        running_task->regs.r10 = r->r10;
        running_task->regs.r11 = r->r11;
        running_task->regs.r12 = r->r12;
        running_task->regs.r13 = r->r13;
        running_task->regs.r14 = r->r14;
        running_task->regs.r15 = r->r15;

        running_task->regs.rflags = r->rflags;
        running_task->regs.rip = r->rip;
        running_task->regs.rsp = r->rsp;

        running_task->regs.cs = r->cs;
        running_task->regs.ss = r->ss;

        running_task->kernel_stack = get_cpu_locals()->thread_kernel_stack;
        running_task->user_stack = get_cpu_locals()->thread_user_stack;

        running_task->regs.cr3 = vmm_get_base();

        running_task->ignore_ring = get_cpu_locals()->ignore_ring;

        asm volatile("fxsave %0;"::"m"(running_task->sse_region)); // save sse

        running_task->regs.mxcsr = get_mxcsr();
        running_task->regs.fcw = get_fcw();

        running_task->tsc_stopped = read_tsc();
        running_task->tsc_total += running_task->tsc_stopped - running_task->tsc_started;
        
        running_task->cpu = -1;

        if (running_task->running) {
            running_task->running = 0;
        }

        /* If we were previously running the task, then it is ready again since we are switching */
        if (running_task->state == RUNNING && running_task->tid != get_cpu_locals()->idle_tid) {
            running_task->state = READY;
            assert(running_task->state == READY);
        }
    }

    int64_t tid_run;
    int64_t picked_first = -1;
repick_task:
    // Run the next thread
    tid_run = pick_task();
    if (tid_run == -1) {
        /* Idle */
        get_cpu_locals()->current_thread = threads[get_cpu_locals()->idle_tid];
        running_task = get_cur_thread();
        goto picked;
    } else {
        if (picked_first == -1) {
            picked_first = tid_run;
        } else if (picked_first == tid_run) {
            tid_run = -1;
            get_cpu_locals()->current_thread = threads[get_cpu_locals()->idle_tid];
            running_task = get_cur_thread();
            goto picked;
        }
        thread_t *to_run = threads[tid_run];
        get_cpu_locals()->current_thread = to_run;
        running_task = get_cur_thread();

        if (to_run->state == WAIT_EVENT) {
            if (*to_run->event) {
                atomic_dec((uint32_t *) to_run->event);
                to_run->state = READY;
                goto picked;
            } else {
                goto repick_task;
            }
        } else if (to_run->state == WAIT_EVENT_TIMEOUT) {
            if (*to_run->event) {
                atomic_dec((uint32_t *) to_run->event);
                to_run->state = READY;
                goto picked;
            } else if (to_run->event_wait_start - global_ticks >= to_run->event_timeout) {
                to_run->state = READY;
                goto picked;
            } else {
                goto repick_task;
            }
        }
    }

picked:
    if (tid_run != -1) {
        assert(running_task->state == READY);
        assert(running_task->running == 0);
        running_task->running = 1;
        running_task->state = RUNNING;
        assert(running_task->state == RUNNING);

        if (get_cpu_locals()->currently_idle) {
            get_cpu_locals()->idle_tsc_count += read_tsc() - get_cpu_locals()->idle_start_tsc;
            used_to_be_idle = 1;
        }

        get_cpu_locals()->currently_idle = 0;
    } else {
        if (!get_cpu_locals()->currently_idle) {
            get_cpu_locals()->active_tsc_count += read_tsc() - get_cpu_locals()->active_start_tsc;
            used_to_be_active = 1;
        }

        get_cpu_locals()->currently_idle = 1;
    }

    get_cpu_locals()->local_dr7 = create_local_dr7(get_cur_thread());
    set_debug_state();

    running_task->cpu = (int) get_cpu_locals()->cpu_index;

    r->rax = running_task->regs.rax;
    r->rbx = running_task->regs.rbx;
    r->rcx = running_task->regs.rcx;
    r->rdx = running_task->regs.rdx;
    r->rbp = running_task->regs.rbp;
    r->rdi = running_task->regs.rdi;
    r->rsi = running_task->regs.rsi;
    r->r8 = running_task->regs.r8;
    r->r9 = running_task->regs.r9;
    r->r10 = running_task->regs.r10;
    r->r11 = running_task->regs.r11;
    r->r12 = running_task->regs.r12;
    r->r13 = running_task->regs.r13;
    r->r14 = running_task->regs.r14;
    r->r15 = running_task->regs.r15;

    r->rflags = running_task->regs.rflags;
    r->rip = running_task->regs.rip;
    r->rsp = running_task->regs.rsp;

    get_cpu_locals()->ignore_ring = running_task->ignore_ring;

    asm volatile("fxrstor %0;"::"m"(running_task->sse_region)); // restore sse
    set_mxcsr(running_task->regs.mxcsr);
    set_fcw(running_task->regs.fcw);

    r->cs = running_task->regs.cs;
    r->ss = running_task->regs.ss;

    write_msr(0xC0000100, running_task->regs.fs); // Set FS.base

    get_cpu_locals()->thread_kernel_stack = running_task->kernel_stack;
    get_cpu_locals()->thread_user_stack = running_task->user_stack;

    if (vmm_get_base() != running_task->regs.cr3) {
        vmm_set_base(running_task->regs.cr3);
    }

    if (get_cpu_locals()->currently_idle) {
        if (!used_to_be_active) {
            get_cpu_locals()->idle_tsc_count += read_tsc() - get_cpu_locals()->idle_start_tsc;
        }
        get_cpu_locals()->idle_start_tsc = read_tsc();
    } else {
        if (!used_to_be_idle) {
            get_cpu_locals()->active_tsc_count += read_tsc() - get_cpu_locals()->active_start_tsc;
        }
        get_cpu_locals()->active_start_tsc = read_tsc();
    }

    running_task->tsc_started = read_tsc();

    get_cpu_locals()->total_tsc = read_tsc();

    interrupt_safe_unlock(sched_lock);
}

int kill_task(int64_t tid) {
    urm_kill_thread_data data;
    data.tid = tid;
    if (tid == get_cur_thread()->tid) {
        send_urm_request_async(&data, URM_KILL_THREAD); // blah
        while (1) { asm("hlt"); } // wait for death
    } else {
        send_urm_request(&data, URM_KILL_THREAD);
    }
    
    return 0;
}

int kill_process(int64_t pid) {
    urm_kill_process_data data;
    data.pid = pid;
    if (pid == get_cur_pid()) {
        send_urm_request_async(&data, URM_KILL_PROCESS);
        while (1) { asm("hlt"); }
    } else {
        send_urm_request(&data, URM_KILL_PROCESS);
    }
    return 0;
}


int map_user_memory(int pid, void *phys, void *virt, uint64_t size, uint16_t perms) {
    interrupt_safe_lock(sched_lock);
    size = ((size + 0x1000 - 1) / 0x1000);
    process_t *process = processes[pid];
    if (!process) return -1;

    void *cr3 = (void *) process->cr3;
    int ret = vmm_map_pages(phys, virt, cr3, size, perms | VMM_PRESENT | VMM_USER);
    interrupt_safe_unlock(sched_lock);
    return ret;
}

int unmap_user_memory(int pid, void *virt, uint64_t size) {
    interrupt_safe_lock(sched_lock);
    size = ((size + 0x1000 - 1) / 0x1000);
    process_t *process = processes[pid];
    if (!process) return -1;

    void *cr3 = (void *) process->cr3;
    int ret = vmm_unmap_pages(virt, cr3, size);
    interrupt_safe_unlock(sched_lock);
    return ret;
}

