#ifndef AMD64_MSR_H
#define AMD64_MSR_H
#include <stdint.h>

uint64_t read_msr(uint64_t msr);
void write_msr(uint64_t msr, uint64_t data);

#endif