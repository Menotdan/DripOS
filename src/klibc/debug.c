#include "debug.h"
#include "logger.h"
#include "lock.h"
#include "sys/smp.h"
#include "sys/apic.h"
#include "mm/vmm.h"

uint64_t dr0 = 0;
uint64_t dr1 = 0;
uint64_t dr2 = 0;
uint64_t dr3 = 0;
uint64_t dr7 = 0;

lock_t watchpoint_lock = {0, 0, 0, 0};
uint8_t watchpoint_cpu_count = 0;

uint64_t read_debug_register(char reg) {
    uint64_t ret = 0;

    switch (reg) {
        case '0':
            asm volatile("mov %%dr0, %0" : "=r"(ret));
            break;
        case '1':
            asm volatile("mov %%dr1, %0" : "=r"(ret));
            break;
        case '2':
            asm volatile("mov %%dr2, %0" : "=r"(ret));
            break;
        case '3':
            asm volatile("mov %%dr3, %0" : "=r"(ret));
            break;
        case '4':
            asm volatile("mov %%dr4, %0" : "=r"(ret));
            break;
        case '5':
            asm volatile("mov %%dr5, %0" : "=r"(ret));
            break;
        case '6':
            asm volatile("mov %%dr6, %0" : "=r"(ret));
            break;
        case '7':
            asm volatile("mov %%dr7, %0" : "=r"(ret));
            break;
        
        default:
            break;
    }
    return ret;
}

void write_debug_register(char reg, uint64_t data) {
    switch (reg) {
        case '0':
            asm volatile("mov %0, %%dr0" :: "r"(data));
            break;
        case '1':
            asm volatile("mov %0, %%dr1" :: "r"(data));
            break;
        case '2':
            asm volatile("mov %0, %%dr2" :: "r"(data));
            break;
        case '3':
            asm volatile("mov %0, %%dr3" :: "r"(data));
            break;
        case '4':
            asm volatile("mov %0, %%dr4" :: "r"(data));
            break;
        case '5':
            asm volatile("mov %0, %%dr5" :: "r"(data));
            break;
        case '6':
            asm volatile("mov %0, %%dr6" :: "r"(data));
            break;
        case '7':
            asm volatile("mov %0, %%dr7" :: "r"(data & ~(0xFFFFFFFF00000000)));
            break;
        
        default:
            break;
    }
}

void init_debugger() {
    dr7 = read_debug_register('7'); // Initial state of debug register
}

uint64_t create_local_dr7(thread_t *t) {
    if (!t->parent) {
        return 0; // nothing lol
    }

    uint64_t ret = 0;
    sprintf("t = %lx", &t);
    sprintf("t->parent = %lx", &(t->parent));
    sprintf("t->parent->local_watchpoint = %lx", &(t->parent->local_watchpoint1_active));
    if (t->parent->local_watchpoint1_active) {
        ret |= 0b1101 << 24;
        ret |= 1 << 5;
    }

    if (t->parent->local_watchpoint2_active) {
        ret |= 0b1101 << 28;
        ret |= 1 << 7;
    }

    return ret;
}

void set_debug_state() {
    lock(watchpoint_lock);

    write_debug_register('0', dr0);
    write_debug_register('1', dr1);
    if (get_cpu_locals()->current_thread->parent) {
        write_debug_register('2', get_cpu_locals()->current_thread->parent->local_watchpoint1);
        write_debug_register('3', get_cpu_locals()->current_thread->parent->local_watchpoint2);
    } else {
        write_debug_register('2', dr2);
        write_debug_register('3', dr3);
    }
    get_cpu_locals()->local_dr7 = create_local_dr7(get_cpu_locals()->current_thread);
    write_debug_register('7', dr7 | get_cpu_locals()->local_dr7);
    watchpoint_cpu_count++;

    unlock(watchpoint_lock);
}

void safe_set_debug_state() {
    interrupt_state_t int_lock = interrupt_lock();
    set_debug_state();
    interrupt_unlock(int_lock);
}

void set_watchpoint(void *address, uint8_t watchpoint_index) {
    if (watchpoint_index == 0) {
        dr0 = (uint64_t) address;
    } else if (watchpoint_index == 1) {
        dr1 = (uint64_t) address;
    } else {
        return;
    }

    dr7 |= (0b1101) << (16 + (watchpoint_index * 4));
    dr7 |= 2 << (watchpoint_index * 2);

    interrupt_state_t int_lock = interrupt_lock();
    lock(watchpoint_lock);
    watchpoint_cpu_count = 0;
    madt_ent0_t **cpus = (madt_ent0_t **) vector_items(&cpu_vector);
    for (uint64_t i = 0; i < cpu_vector.items_count; i++) {
        if (i < cpu_vector.items_count) {
            if ((cpus[i]->cpu_flags & 1 || cpus[i]->cpu_flags & 2) && cpus[i]->apic_id != get_lapic_id()) {
                send_ipi(cpus[i]->apic_id, (1 << 14) | 250); // Send interrupt 250
            }
        }
    }
    unlock(watchpoint_lock);
    set_debug_state();
    interrupt_unlock(int_lock);

    while (watchpoint_cpu_count < cores_booted) {
        asm volatile ("pause");
    }
}

void clear_global_watchpoint(uint8_t watchpoint_index) {
    dr7 &= ~((0b1101) << (16 + (watchpoint_index * 4)));
    dr7 &= ~(2 << (watchpoint_index * 2));

    madt_ent0_t **cpus = (madt_ent0_t **) vector_items(&cpu_vector);
    for (uint64_t i = 0; i < cpu_vector.items_count; i++) {
        if (i < cpu_vector.items_count) {
            if ((cpus[i]->cpu_flags & 1 || cpus[i]->cpu_flags & 2) && cpus[i]->apic_id != get_lapic_id()) {
                send_ipi(cpus[i]->apic_id, (1 << 14) | 250); // Send interrupt 250
            }
        }
    }

    safe_set_debug_state();
}

void send_reload_debug() {
    madt_ent0_t **cpus = (madt_ent0_t **) vector_items(&cpu_vector);
    for (uint64_t i = 0; i < cpu_vector.items_count; i++) {
        if (i < cpu_vector.items_count) {
            if ((cpus[i]->cpu_flags & 1 || cpus[i]->cpu_flags & 2) && cpus[i]->apic_id != get_lapic_id()) {
                send_ipi(cpus[i]->apic_id, (1 << 14) | 250); // Send interrupt 250
            }
        }
    }

    safe_set_debug_state();
}

void set_local_watchpoint(void *address, uint8_t watchpoint_index) {
    if (watchpoint_index == 0) {
        get_cpu_locals()->current_thread->parent->local_watchpoint1 = (uint64_t) address;
        get_cpu_locals()->current_thread->parent->local_watchpoint1_active = 1;
    } else if (watchpoint_index == 1) {
        get_cpu_locals()->current_thread->parent->local_watchpoint2 = (uint64_t) address;
        get_cpu_locals()->current_thread->parent->local_watchpoint2_active = 1;
    }

    safe_set_debug_state();
}

void stack_trace(int_reg_t *r, uint64_t max_frames) {
    log("");
    log("{-Trace-} Stack trace begin.");
    uint64_t frames_parsed = 0;

    uint64_t rbp = (uint64_t) kernel_address((void *) r->rbp);
    while (1) {
        uint64_t *stack = (uint64_t *) (rbp);

        /* Read the rbp and rsp from the stack */
        if (!stack[0]) {
            log("{-Trace-} Stack trace end.");
            log("");
            break;
        }

        rbp = (uint64_t) kernel_address((void *) stack[0]);
        log("{-Trace-} RIP: %lx, RBP: %lx", stack[1], stack[0]);

        frames_parsed++;
        if (frames_parsed == max_frames) {
            log("{-Trace-} Stack trace end. (Max frames)");
            log("");
            break;
        }
    }
}

void debug_handler(int_reg_t *r) {
    log("{-Debug-} Debug triggered.");
    log("{-Debug-} DR6: %lx", read_debug_register('6'));
    log("{-Debug-} DR7: %lx", read_debug_register('7'));
    log("{-Debug-} RIP: %lx", r->rip);
    log("{-Debug-} PID: %ld", get_cpu_locals()->current_thread->parent_pid);
    stack_trace(r, 10);
}