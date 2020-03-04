#include "idt.h"
#include "sys/smp.h"

void set_idt_gate(uint8_t n, uint64_t handler) {
    uint16_t low = (uint16_t) (handler & 0xFFFF); // Get the low 16 bits of the function
    uint16_t mid = (uint16_t) ((handler & (0xFFFF << 16)) >> 16); // Get the middle 16 bits
    uint32_t high = (uint32_t) ((handler & ((uint64_t) 0xFFFFFFFF << 32)) >> 32); // Get the high 32 bits

    /* Set the entry */
    idt_gate_t *idt_entries = get_cpu_locals()->idt;
    idt_entries[n].low_offset = low;
    idt_entries[n].selector = 0x8;
    idt_entries[n].always0 = 0;
    idt_entries[n].ist = 0;
    idt_entries[n].type = 0b10001110; // Low four bits indicate interrupt handler, bit 7 is the present bit,
                                        // bits 6 and 5 are the privelege level and bit 4 is always 0 for interrupts
    idt_entries[n].middle_offset = mid;
    idt_entries[n].high_offset = high;
}

void set_ist(uint8_t n, uint8_t ist_index) {
    idt_gate_t *idt_entries = get_cpu_locals()->idt;
    idt_entries[n].ist = (ist_index & 0b111);
}

void load_idt() {
    idt_reg.base = (uint64_t) get_cpu_locals()->idt;
    idt_reg.limit = IDT_ENTRIES * sizeof(idt_gate_t) - 1;
    asm volatile("lidtq (%0)" :: "r"(&idt_reg));
}