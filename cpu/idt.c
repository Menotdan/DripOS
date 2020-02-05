#include "idt.h"

void set_idt_gate(int n, uint64_t handler) {
    uint16_t low = (uint16_t)(handler & 0xFFFF); // Get the low 16 bits of the function
    uint16_t mid = (uint16_t)((handler & (0xFFFF << 16)) >> 16); // Get the middle 16 bits
    uint32_t high =
        (uint32_t)((handler & ((uint64_t)0xFFFFFFFF << 32)) >> 32); // Get the high 32 bits
    idt[n].low_offset = low;
    idt[n].sel = KERNEL_CS;
    idt[n].always0 = 0;
    idt[n].end0 = 0;
    idt[n].flags = 0x8E;
    idt[n].middle_offset = mid;
    idt[n].high_offset = high;
}

void set_idt() {
    idt_reg.base = (uint64_t)&idt;
    idt_reg.limit = IDT_ENTRIES * sizeof(idt_gate_t) - 1;
    /* Don't make the mistake of loading &idt -- always load &idt_reg */
    __asm__ __volatile__("lidtq (%0)" : : "r"(&idt_reg));
}
