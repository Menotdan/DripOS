#ifndef SMP_H
#define SMP_H
#include <stdint.h>
#include "proc/scheduler.h"

typedef struct {
    /* Needed. Do NOT remove */
    uint64_t meta_pointer;
    /* Change these ig lol */
    uint8_t apic_id;
    uint8_t cpu_index;
    task_t *current_thread;
    int64_t pid;
    int64_t tid;

    uint64_t idle_tsc_count;
    uint64_t idle_start_tsc;
    uint64_t idle_end_tsc;
} __attribute__((packed)) cpu_locals_t;

void launch_cpus();
void send_ipi(uint8_t ap, uint32_t ipi_number);
uint8_t get_cpu_count();
cpu_locals_t *get_cpu_locals();

#endif