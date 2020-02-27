#ifndef SMP_H
#define SMP_H
#include <stdint.h>

void launch_cpus();
void send_ipi(uint8_t ap, uint32_t ipi_number);
uint8_t get_cpu_count();

#endif