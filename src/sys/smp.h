#ifndef SMP_H
#define SMP_H
#include <stdint.h>
#include "proc/scheduler.h"
#include "sys/int/idt.h"
#include "sys/tss.h"

typedef struct {
    /* Needed. Do NOT remove or change positions. */
    uint64_t meta_pointer;
    uint64_t thread_kernel_stack;
    uint64_t thread_user_stack;
    uint64_t in_irq;

    /* Change these ig lol */
    uint8_t apic_id;
    uint8_t cpu_index;
    thread_t *current_thread;
    int64_t pid;
    int64_t tid;
    int64_t idle_tid; // The TID for this CPUs idle task

    uint64_t idle_tsc_count;
    uint64_t idle_start_tsc;
    uint64_t idle_end_tsc;
    uint64_t currently_idle;

    uint64_t active_tsc_count;
    uint64_t active_start_tsc;
    uint64_t active_end_tsc;

    uint64_t total_tsc;

    uint64_t local_dr7;

    idt_gate_t idt[IDT_ENTRIES];
    tss_64_t tss;

    uint8_t ignore_ring;
} __attribute__((packed)) cpu_locals_t;

hashmap_t *cpu_locals_list;

void launch_cpus();
void send_ipi(uint8_t ap, uint32_t ipi_number);
cpu_locals_t *get_cpu_locals();
void new_cpu_locals();
uint8_t get_lapic_id();

extern uint8_t cores_booted;

#endif