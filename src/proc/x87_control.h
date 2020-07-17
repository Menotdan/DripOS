#ifndef x87_CONTROL_H
#define x87_CONTROL_H
#include <stdint.h>

static inline uint16_t get_fcw() {
    uint16_t buf;
    asm volatile("fnstcw (%0)" :: "r"(&buf) : "memory");
    return buf;
}

static inline void set_fcw(uint16_t buf) {
    asm volatile("fldcw (%0)" :: "r"(&buf) : "memory");
}

#endif