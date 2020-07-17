#ifndef MXCSR_H
#define MXCSR_H
#include <stdint.h>

static inline uint32_t get_mxcsr() {
    uint32_t buf;
    asm volatile("stmxcsr (%0)" :: "r"(&buf) : "memory");
    return buf;
}

static inline void set_mxcsr(uint32_t buf) {
    asm volatile("ldmxcsr (%0)" :: "r"(&buf) : "memory");
}

#endif