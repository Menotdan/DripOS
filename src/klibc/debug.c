#include "debug.h"
#include "sys/smp.h"
#include "sys/apic.h"

uint64_t dr0 = 0;
uint64_t dr1 = 0;
uint64_t dr2 = 0;
uint64_t dr3 = 0;
uint64_t dr7 = 0;

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
            asm volatile("mov %0, %%dr7" :: "r"(data));
            break;
        
        default:
            break;
    }
}

void init_debugger() {
    dr7 = read_debug_register('7'); // Initial state of debug register
}

void set_debug_state() {
    write_debug_register('0', dr0);
    write_debug_register('1', dr1);
    write_debug_register('2', dr2);
    write_debug_register('3', dr3);
    write_debug_register('7', dr7);
}

void set_watchpoint(void *address, uint8_t watchpoint_index) {
    if (watchpoint_index == 0) {
        dr0 = (uint64_t) address;
    } else if (watchpoint_index == 1) {
        dr1 = (uint64_t) address;
    } else if (watchpoint_index == 2) {
        dr2 = (uint64_t) address;
    } else if (watchpoint_index == 3) {
        dr3 = (uint64_t) address;
    }

    dr7 |= (0b0101) << (16 + (watchpoint_index * 4));

    madt_ent0_t **cpus = (madt_ent0_t **) vector_items(&cpu_vector);
    for (uint64_t i = 0; i < cpu_vector.items_count; i++) {
        if (i < cpu_vector.items_count) {
            if ((cpus[i]->cpu_flags & 1 || cpus[i]->cpu_flags & 2) && cpus[i]->apic_id != get_lapic_id()) {
                send_ipi(cpus[i]->apic_id, (1 << 14) | 250); // Send interrupt 253
            }
        }
    }
    set_debug_state();
}

void debug_handler() {
    sprintf("DR6: %lx\n", read_debug_register('6'));
    while (1) {
        
    }
}