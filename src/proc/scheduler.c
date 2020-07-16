#include "scheduler.h"
#include "safe_userspace.h"
#include "klibc/stdlib.h"
#include "klibc/string.h"
#include "klibc/queue.h"
#include "klibc/errno.h"
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
lock_t scheduler_lock = {0, 0, 0, 0};
interrupt_safe_lock_t sched_lock = {0, 0, 0, 0, -1};

task_regs_t default_kernel_regs = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0x10,0x8,0,0x202,0};
task_regs_t default_user_regs = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0x23,0x1B,0,0x202,0};

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

void start_idle() {
    get_cpu_locals()->idle_start_tsc = read_tsc();
}

void end_idle() {
    cpu_locals_t *cpu_locals = get_cpu_locals();
    cpu_locals->idle_end_tsc = read_tsc();
    cpu_locals->idle_tsc_count += (cpu_locals->idle_end_tsc - cpu_locals->idle_start_tsc);
}

void _idle() {
    while (1) {
        asm volatile("hlt");
    }
}

/* Initialize the BSP for scheduling */
void scheduler_init_bsp() {
    /* Setup syscall MSRs for this CPU */
    write_msr(0xC0000081, read_msr(0xC0000081) | ((uint64_t) 0x8 << 32));
    write_msr(0xC0000081, read_msr(0xC0000081) | ((uint64_t) 0x18 << 48));
    write_msr(0xC0000082, (uint64_t) syscall_stub); // Start execution at the syscall stub when a syscall occurs
    write_msr(0xC0000084, 0); // Mask nothing
    write_msr(0xC0000080, read_msr(0xC0000080) | 1); // Set the syscall enable bit

    /* Setup the idle thread */
    uint64_t idle_rsp = (uint64_t) kcalloc(0x1000) + 0x1000;
    thread_t *new_idle = create_thread("Idle thread", _idle, idle_rsp, 0);
    new_idle->state = BLOCKED; // Idle thread will *not* run unless provoked
    int64_t idle_tid = start_thread(new_idle);

    get_cpu_locals()->idle_tid = idle_tid;
    sprintf("Idle task: %ld\n", get_cpu_locals()->idle_tid);
}

/* Initilialize an AP for scheduling */
void scheduler_init_ap() {
    /* Setup syscall MSRs for this CPU */
    write_msr(0xC0000081, read_msr(0xC0000081) | ((uint64_t) 0x8 << 32));
    write_msr(0xC0000081, read_msr(0xC0000081) | ((uint64_t) 0x18 << 48));
    write_msr(0xC0000082, (uint64_t) syscall_stub); // Start execution at the syscall stub when a syscall occurs
    write_msr(0xC0000084, 0);
    write_msr(0xC0000080, read_msr(0xC0000080) | 1); // Set the syscall enable bit

    /* Setup the idle thread */
    uint64_t idle_rsp = (uint64_t) kcalloc(0x1000) + 0x1000;
    thread_t *new_idle = create_thread("Idle thread", _idle, idle_rsp, 0);
    new_idle->state = BLOCKED; // Idle thread will *not* run unless provoked
    int64_t idle_tid = start_thread(new_idle);

    get_cpu_locals()->idle_tid = idle_tid;
    sprintf("Idle task: %ld\n", get_cpu_locals()->idle_tid);
}

/* Create a new thread *and* add it to the dynarray */
int64_t new_thread(char *name, void (*main)(), uint64_t rsp, int64_t pid, uint8_t ring) {
    thread_t *new_task = create_thread(name, main, rsp, ring);
    int64_t new_tid = add_new_child_thread(new_task, pid);
    return new_tid;
}

/* Add the thread to the dynarray */
int64_t start_thread(thread_t *thread) {
    interrupt_safe_lock(sched_lock);
    int64_t tid = -1;
    for (uint64_t i = 0; i < threads_list_size; i++) {
        if (!threads[i]) {
            tid = i;
            break;
        }
    }
    if (tid == -1) {
        threads = krealloc(threads, (threads_list_size + 10) * sizeof(thread_t *));
        tid = threads_list_size;
        threads_list_size += 10;
    }
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

    /* Create null argv, enviroment, and auxv */
    new_task->vars.envc = 0;
    new_task->vars.argc = 0;
    new_task->vars.auxc = 0;
    new_task->vars.enviroment = NULL;
    new_task->vars.auxv = NULL;
    new_task->vars.argv = NULL;

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

    int64_t index = -1;

    /* Find the parent process */
    process_t *new_parent = (void *) 0;
    if ((uint64_t) pid < process_list_size) {
        new_parent = processes[pid];
    }
    if (!new_parent) { sprintf("[Scheduler] Couldn't find parent\n"); return -1; }

    /* Store the new task and save its TID */

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
    threads[new_tid] = thread;

    thread->tid = new_tid;
    thread->regs.cr3 = new_parent->cr3; // Inherit parent's cr3
    thread->parent_pid = pid;

    /* Add the TID to it's parent's threads list */
    for (uint64_t i = 0; i < new_parent->threads_size; i++) {
        if (!new_parent->threads[i]) {
            index = i;
            break;
        }
    }
    if (new_tid == -1) {
        threads = krealloc(new_parent->threads, (new_parent->threads_size + 10) * sizeof(int64_t));
        index = new_parent->threads_size;
        new_parent->threads_size += 10;
    }
    new_parent->threads[index] = thread->tid;

    interrupt_safe_unlock(sched_lock);
    return new_tid;
}

/* Add a new thread to the dynarray and as a child of a process */
int64_t add_new_child_thread(thread_t *thread, int64_t pid) {
    interrupt_safe_lock(sched_lock);

    int64_t index = -1;

    /* Find the parent process */
    process_t *new_parent = (void *) 0;
    if ((uint64_t) pid < process_list_size) {
        new_parent = processes[pid];
    }
    if (!new_parent) { sprintf("[Scheduler] Couldn't find parent\n"); return -1; }

    /* Store the new task and save its TID */

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
    threads[new_tid] = thread;

    thread->tid = new_tid;
    thread->regs.cr3 = new_parent->cr3; // Inherit parent's cr3
    thread->parent_pid = pid;

    if (new_parent->threads_size == 0) { // No children
        // -1 because kmalloc creates boundaries so page might not be mapped :|
        void *phys = virt_to_phys((void *) (thread->regs.rsp - 1), (pt_t *) thread->regs.cr3);
        if ((uint64_t) phys == 0xFFFFFFFFFFFFFFFF) {
            sprintf("RSP not mapped :thonk:\n");
            goto done;
        }
        sprintf("virt: %lx\n", thread->regs.rsp);
        sprintf("phys: %lx\n", phys);
        uint64_t stack = GET_HIGHER_HALF(uint64_t, phys) + 1;
        sprintf("stack var: %lx\n", stack);
        uint64_t old_stack = stack;

        int argc = thread->vars.argc;
        char **argv = thread->vars.argv;

        /* Add argc, argv, and auxv */

        // Actual data
        uint64_t *string_offsets = kcalloc(sizeof(uint64_t) * argc + sizeof(uint64_t) * thread->vars.envc);
        uint64_t total_string_len = 0;
        sprintf("argc: %d\n", argc);
        for (int i = 0; i < argc; i++) {
            sprintf("i = %d with argv[i] = %lx\n", i, argv[i]);

            total_string_len += (strlen(argv[i]) + 1);
            string_offsets[i] = total_string_len;

            char *new_string = (char *) (old_stack - string_offsets[i]);
            sprintf("String Data: %s, String: %lx, string offset: %lu\n", argv[i], new_string, string_offsets[i]);
            strcpy(argv[i], new_string);
        }
        asm("hlt");
        for (int i = argc; i < argc + thread->vars.envc; i++) {
            sprintf("i = %d with envp[i] = %lx\n", i, thread->vars.enviroment[i - argc]);
            total_string_len += (strlen(thread->vars.enviroment[i - argc]) + 1);
            string_offsets[i] = total_string_len;

            char *new_string = (char *) (old_stack - string_offsets[i]);
            sprintf("String Data: %s, String: %lx, string offset: %lu\n", thread->vars.enviroment[i - argc], new_string, string_offsets[i]);
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
done:
    /* Add the TID to it's parent's threads list */
    for (uint64_t i = 0; i < new_parent->threads_size; i++) {
        if (!new_parent->threads[i]) {
            index = i;
            break;
        }
    }
    if (new_tid == -1) {
        threads = krealloc(new_parent->threads, (new_parent->threads_size + 10) * sizeof(int64_t));
        index = new_parent->threads_size;
        new_parent->threads_size += 10;
    }
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
    new_process->current_brk = 0x10000000000;
    new_process->ipc_handles = init_hashmap();
    strcpy(name, new_process->name);

    return new_process;
}


/* Add a process to the process list */
int64_t add_process(process_t *process) {
    interrupt_safe_lock(sched_lock);

    /* Add element to the dynarray and save the PID */
    int64_t pid = -1;
    for (uint64_t i = 0; i < process_list_size; i++) {
        if (!processes[i]) {
            pid = i;
            break;
        }
    }
    if (pid == -1) {
        processes = krealloc(processes, (process_list_size + 10) * sizeof(process_t *));
        pid = process_list_size;
        process_list_size += 10;
    }
    processes[pid] = process;

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

int64_t pick_task() {
    int64_t tid_ret = -1;

    if (!(get_cpu_locals()->current_thread)) {
        return tid_ret;
    }

    /* Prioritze tasks right after the current task */
    for (int64_t t = get_cpu_locals()->current_thread->tid + 1; (uint64_t) t < threads_list_size; t++) {
        //sprintf("Looking at t = %ld in first loop\n", t);
        thread_t *task = threads[t];
        if (task) {
            if (task->state == READY || task->state == WAIT_EVENT || task->state == WAIT_EVENT_TIMEOUT) {
                tid_ret = task->tid;
                //sprintf("picked in first loop with state %u and event %lx\n", task->state, task->event);
                return tid_ret;
            }
            if (task->state == SLEEP) {
                //sprintf("Time left: %lu\n", task->sleep_node->time_left);
            }
        }
    }

    /* Then look at the rest of the tasks */
    for (int64_t t = 0; t < get_cpu_locals()->current_thread->tid + 1; t++) {
        //sprintf("Looking at t = %ld in second loop\n", t);
        thread_t *task = threads[t];
        if (task) {
            if (task->state == READY || task->state == WAIT_EVENT || task->state == WAIT_EVENT_TIMEOUT) {
                tid_ret = task->tid;
                //sprintf("picked in second loop with state %u and event %lx\n", task->state, task->event);
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

void schedule_bsp(int_reg_t *r) {
    send_scheduler_ipis();

    if (!spinlock_check_and_lock(&sched_lock.lock_dat)) {
        sched_lock.current_holder = __FUNCTION__;
        schedule(r);
    } else {
        //sprintf("Scheduler was locked~! By %s.\n", sched_lock.current_holder);
    }
}

void schedule_ap(int_reg_t *r) {
    if (!spinlock_check_and_lock(&sched_lock.lock_dat)) {
        sched_lock.current_holder = __FUNCTION__;
        schedule(r);
    } else {
        //sprintf("Scheduler was locked~! By %s.\n", sched_lock.current_holder);
    }
    scheduler_ran = 1;
}

void schedule(int_reg_t *r) {
    thread_t *running_task = get_cpu_locals()->current_thread;
    if (running_task) {
        if (running_task->tid == get_cpu_locals()->idle_tid) {
            end_idle(); // End idling timer for this CPU
        }

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

        running_task->regs.cr3 = vmm_get_pml4t();

        asm volatile("fxsave %0;"::"m"(running_task->sse_region)); // save sse

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
    //sprintf("Picked %ld\n", tid_run);
    if (tid_run == -1) {
        /* Idle */
        get_cpu_locals()->current_thread = threads[get_cpu_locals()->idle_tid];
        running_task = get_cpu_locals()->current_thread;
    } else {
        if (picked_first == -1) {
            picked_first = tid_run;
        } else if (picked_first == tid_run) {
            // welp
            tid_run = -1;
            get_cpu_locals()->current_thread = threads[get_cpu_locals()->idle_tid];
            running_task = get_cpu_locals()->current_thread;
            goto picked;
        }
        thread_t *to_run = threads[tid_run];
        get_cpu_locals()->current_thread = to_run;
        running_task = get_cpu_locals()->current_thread;
        if (to_run->state == WAIT_EVENT) {
            if (*to_run->event) {
                atomic_dec((uint32_t *) to_run->event);
                to_run->state = READY;
            } else {
                goto repick_task;
            }
        } else if (to_run->state == WAIT_EVENT_TIMEOUT) {
            if (*to_run->event) {
                atomic_dec((uint32_t *) to_run->event);
                to_run->state = READY;
            } else if (to_run->event_wait_start - global_ticks >= to_run->event_timeout) {
                to_run->state = READY;
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
    }

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

    asm volatile("fxrstor %0;"::"m"(running_task->sse_region)); // restore sse

    r->cs = running_task->regs.cs;
    r->ss = running_task->regs.ss;

    write_msr(0xC0000100, running_task->regs.fs); // Set FS.base

    get_cpu_locals()->thread_kernel_stack = running_task->kernel_stack;
    get_cpu_locals()->thread_user_stack = running_task->user_stack;

    if (vmm_get_pml4t() != running_task->regs.cr3) {
        vmm_set_pml4t(running_task->regs.cr3);
    }

    running_task->tsc_started = read_tsc();

    if (tid_run == -1) {
        start_idle(); // Start idling timer
    }

    get_cpu_locals()->total_tsc = read_tsc();

    interrupt_safe_unlock(sched_lock);
}

int kill_task(int64_t tid) {
    urm_kill_thread_data data;
    data.tid = tid;
    if (tid == get_cpu_locals()->current_thread->tid) {
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
    if (pid == get_cpu_locals()->current_thread->parent_pid) {
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

void *psuedo_mmap(void *base, uint64_t len, syscall_reg_t *r) {
    interrupt_safe_lock(sched_lock);
    len = (len + 0x1000 - 1) / 0x1000;
    process_t *process = processes[get_cpu_locals()->current_thread->parent_pid];
    if (!process) { r->rdx = ESRCH; interrupt_safe_unlock(sched_lock); return (void *) 0; } // bruh

    void *phys = pmm_alloc(len * 0x1000);
    memset(GET_HIGHER_HALF(void *, phys), 0, len * 0x1000);

    if (base) {
        uint64_t mapped = 0;
        int map_err = 0;
        for (uint64_t i = 0; i < len; i++) {
            int err = vmm_map_pages(phys, (void *) ((uint64_t) base + i * 0x1000), 
                (void *) process->cr3, 1, 
                VMM_WRITE | VMM_USER | VMM_PRESENT);
            phys = (void *) ((uint64_t) phys + 0x1000);
            
            if (err) {
                mapped = i;
                map_err = 1;
                break;
            }
        }

        if (map_err) {
            /* Unmap the error */
            for (uint64_t i = 0; i < mapped; i++) {
                vmm_unmap_pages((void *) ((uint64_t) base + i * 0x1000), 
                    (void *) process->cr3, 1);
            }
            phys = (void *) ((uint64_t) phys - (0x1000 * mapped));
            r->rdx = ENOMEM;
            pmm_unalloc(phys, len * 0x1000);

            interrupt_safe_unlock(sched_lock);
            return (void *) 0;
        } else {
            void *ret = (void *) base;

            interrupt_safe_unlock(sched_lock);
            return ret;
        }
    } else {
        lock(process->brk_lock);
        uint64_t mapped = 0;
        int map_err = 0;
        for (uint64_t i = 0; i < len; i++) {
            int err = vmm_map_pages(phys, (void *) ((uint64_t) process->current_brk + i * 0x1000), 
                (void *) process->cr3, 1, 
                VMM_WRITE | VMM_USER | VMM_PRESENT);
            
            phys = (void *) ((uint64_t) phys + 0x1000);
            
            if (err) {
                mapped = i;
                map_err = 1;
                break;
            }
        }

        if (map_err) {
            /* Unmap the error */
            for (uint64_t i = 0; i < mapped; i++) {
                vmm_unmap_pages((void *) ((uint64_t) process->current_brk + i * 0x1000), 
                    (void *) process->cr3, 1);
            }
            r->rdx = ENOMEM;
            phys = (void *) ((uint64_t) phys - (0x1000 * mapped));
            pmm_unalloc(phys, len * 0x1000);

            unlock(process->brk_lock);

            interrupt_safe_unlock(sched_lock);
            return (void *) 0;
        } else {
            void *ret = (void *) process->current_brk;
            process->current_brk += len * 0x1000;

            unlock(process->brk_lock);

            interrupt_safe_unlock(sched_lock);
            return ret;
        }
    }
}

int munmap(char *addr, uint64_t len) {
    interrupt_safe_lock(sched_lock);
    len = (len + 0x1000 - 1) / 0x1000;
    process_t *process = processes[get_cpu_locals()->current_thread->parent_pid];
    interrupt_safe_unlock(sched_lock);
    if (!process) { return -ESRCH; } // bruh

    if ((uint64_t) addr != ((uint64_t) addr & ~(0xfff))) {
        return -EINVAL;
    }
    if ((uint64_t) addr > 0x7fffffffffff) {
        // Not user memory, stupid userspace
        return -EINVAL;
    }

    for (uint64_t i = 0; i < len; i++) {
        void *phys = virt_to_phys(addr, (void *) vmm_get_pml4t());
        vmm_unmap(addr, 1);
        if ((uint64_t) phys != 0xffffffffffffffff) {
            pmm_unalloc(phys, 0x1000);
        }
        addr += 0x1000;
    }
    return 0;
}

int fork(syscall_reg_t *r) {
    sprintf("got fork call with r = %lx\n", r);
    interrupt_safe_lock(sched_lock);
    process_t *process = processes[get_cpu_locals()->current_thread->parent_pid]; // Old process
    void *new_cr3 = vmm_fork((void *) process->cr3); // Fork address space

    process_t *forked_process = create_process(process->name, new_cr3);
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
    processes[new_pid] = forked_process;

    process_t *new_process = processes[new_pid]; // Get the process struct
    int64_t old_pid = process->pid;
    interrupt_safe_unlock(sched_lock);

    /* Copy fds */
    clone_fds(old_pid, new_pid);

    interrupt_safe_lock(sched_lock);

    /* Parent id */
    new_process->ppid = process->pid;

    /* Other ids */
    new_process->uid = process->uid;
    new_process->gid = process->gid;
    
    /* Other data about the process */
    lock(process->brk_lock);
    new_process->current_brk = process->current_brk;
    unlock(process->brk_lock);

    /* New thread */
    thread_t *old_thread = get_cpu_locals()->current_thread;

    thread_t *thread = create_thread(old_thread->name, (void (*)()) old_thread->regs.rip, old_thread->regs.rsp, old_thread->ring);
    thread->regs.rax = 0;
    thread->regs.rbx = r->rbx;
    thread->regs.rcx = r->rcx;
    thread->regs.rdx = r->rdx;
    thread->regs.rbp = r->rbp;
    thread->regs.rsi = r->rsi;
    thread->regs.rdi = r->rdi;
    thread->regs.r8 = r->r8;
    thread->regs.r9 = r->r9;
    thread->regs.r10 = r->r10;
    thread->regs.r11 = r->r11;
    thread->regs.r12 = r->r12;
    thread->regs.r13 = r->r13;
    thread->regs.r14 = r->r14;
    thread->regs.r15 = r->r15;
    thread->regs.rsp = get_cpu_locals()->thread_user_stack;
    thread->regs.rip = r->rcx;
    thread->regs.rflags = r->r11;
    memcpy((uint8_t *) old_thread->sse_region, (uint8_t *) thread->sse_region, 512);

    interrupt_safe_unlock(sched_lock);

    add_new_child_thread_no_stack_init(thread, new_pid); // Add the thread

    /* Bye bye */
    return new_pid;
}

void execve(char *executable_path, char **argv, char **envp, syscall_reg_t *r) {
    r->rdx = 0;
    uint64_t argc = 0;
    uint64_t envc = 0;
    int found_null_argv = 0;
    int found_null_envp = 0;

    for (uint64_t i = 0; i < 128; i++) {
        if (!range_mapped((void *) ((uint64_t) argv + (sizeof(char *) * i)), sizeof(char *)) || ((uint64_t) argv + 8 + (sizeof(char *) * i)) > 0x7fffffffffff) {
            r->rdx = EFAULT;
            return;
        }
        if (!argv[i]) {
            found_null_argv = 1;
            break;
        }
        argc++;
    }

    for (uint64_t i = 0; i < 128; i++) {
        if (!range_mapped((void *) ((uint64_t) envp + (sizeof(char *) * i)), sizeof(char *)) || ((uint64_t) envp + 8 + (sizeof(char *) * i)) > 0x7fffffffffff) {
            r->rdx = EFAULT;
            return;
        }
        if (!envp[i]) {
            found_null_envp = 1;
            break;
        }
        envc++;
    }

    if (!found_null_argv || !found_null_envp) {
        r->rdx = EFAULT;
        return;
    }

    char **kernel_argv = kcalloc(sizeof(char *) * argc);
    char **kernel_envp = kcalloc(sizeof(char *) * envc);
    char *kernel_exec_path = (void *) 0;

    for (uint64_t i = 0; i < argc; i++) {
        char *string = argv[i];
        void *kernel_string = check_and_copy_string(string);
        if (!kernel_string) {
            goto fault_return;
        }

        kernel_argv[i] = kernel_string;
    }

    for (uint64_t i = 0; i < envc; i++) {
        char *string = envp[i];
        void *kernel_string = check_and_copy_string(string);
        if (!kernel_string) {
            goto fault_return;
        }

        kernel_envp[i] = kernel_string;
    }

    kernel_exec_path = check_and_copy_string(executable_path);
    if (!kernel_exec_path) {
        goto fault_return;
    }
    urm_execve_data data;
    data.argv = kernel_argv;
    data.envp = kernel_envp;
    data.executable_path = kernel_exec_path;
    data.envc = envc;
    data.argc = argc;
    data.pid = get_cpu_locals()->current_thread->parent_pid;
    data.tid = get_cpu_locals()->current_thread->tid;
    int ret = send_urm_request(&data, URM_EXECVE);
    goto done;

fault_return:
    for (uint64_t x = 0; x < argc; x++) {
        if (kernel_argv[x]) {
            kfree(kernel_argv[x]);
        }
    }
    kfree(kernel_argv);
    for (uint64_t x = 0; x < envc; x++) {
        if (kernel_envp[x]) {
            kfree(kernel_envp[x]);
        }
    }
    kfree(kernel_envp);
    if (kernel_exec_path) {
        kfree(kernel_exec_path);
    }
    r->rdx = EFAULT;
    return;
done:
    sprintf("returning\n");
    for (uint64_t x = 0; x < argc; x++) {
        if (kernel_argv[x]) {
            kfree(kernel_argv[x]);
        }
    }
    kfree(kernel_argv);
    for (uint64_t x = 0; x < envc; x++) {
        if (kernel_envp[x]) {
            kfree(kernel_envp[x]);
        }
    }
    kfree(kernel_envp);
    if (kernel_exec_path) {
        kfree(kernel_exec_path);
    }
    r->rdx = ret; // return error :(
}

void set_fs_base_syscall(uint64_t base) {
    get_cpu_locals()->current_thread->regs.fs = base;
    write_msr(0xC0000100, get_cpu_locals()->current_thread->regs.fs); // Set the FS base in case the scheduler hasn't rescheduled
}