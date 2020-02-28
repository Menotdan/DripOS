#ifndef SMP_H
#define SMP_H
#include <stdint.h>
#include "proc/scheduler.h"
#include "sys/int/idt.h"

typedef struct {

} ipi_flags_t;

typedef struct {
    /* Needed. Do NOT remove */
    uint64_t meta_pointer;
    /* Change these ig lol */
    uint8_t apic_id;
    uint8_t cpu_index;
    task_t *current_thread;
    int64_t pid;
    int64_t tid;
    int64_t idle_tid; // The TID for this CPUs idle task

    uint64_t idle_tsc_count;
    uint64_t idle_start_tsc;
    uint64_t idle_end_tsc;

    uint64_t total_tsc;

    idt_gate_t idt[IDT_ENTRIES];
} __attribute__((packed)) cpu_locals_t;

void launch_cpus();
void send_ipi(uint8_t ap, uint32_t ipi_number);
uint8_t get_cpu_count();
cpu_locals_t *get_cpu_locals();
void new_cpu_locals();
uint8_t get_lapic_id();

#endif