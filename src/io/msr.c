#include "msr.h"
#include <cpuid.h>

uint64_t read_msr(uint64_t msr) {
    uint64_t rax, rdx;
    asm("rdmsr" : "=a" (rax), "=d" (rdx) : "c"(msr));

    return (rdx << 32) | rax;
}

void write_msr(uint64_t msr, uint64_t data) {
    uint64_t rax = data & 0xFFFFFFFF;
    uint64_t rdx = data >> 32;
    asm("wrmsr" :: "a" (rax), "d" (rdx), "c"(msr));
}

uint64_t read_tsc() {
    uint64_t rax, rdx;
    asm("rdtsc" : "=a" (rax), "=d" (rdx));

    return (rdx << 32) | rax;
}
