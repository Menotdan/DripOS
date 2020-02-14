#ifndef IDT_H
#define IDT_H
#include <stdint.h>

/* Segment selectors */
#define KERNEL_CS 0x08
#define KERNEL_DS 0x10
#define USER_CS 0x18
#define USER_DS 0x20

/* How every interrupt gate (handler) is defined */
typedef struct {
    uint16_t low_offset; /* Lowest 16 bits of handler function address */
    uint16_t sel; /* Segment selector given to this interrupt */
    uint8_t always0;
    /* First byte
     * Bit 7: "Interrupt is present"
     * Bits 6-5: Privilege level of caller (0=kernel..3=user)
     * Bit 4: Set to 0 for interrupt gates
     * Bits 3-0: bits 1110 = decimal 14 = "32 bit interrupt gate" */
    uint8_t flags;
    uint16_t middle_offset; /* Middle 16 bits of handler function address */
    uint32_t high_offset; /* Highest 32 bits of handler function address */
    uint32_t end0;
} __attribute__((packed)) idt_gate_t;

/* A pointer to the array of interrupt handlers.
 * Assembly instruction 'lidt' will read it */
typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) idt_register_t;

#define IDT_ENTRIES 256
idt_gate_t idt[IDT_ENTRIES];
idt_register_t idt_reg;

/* Functions implemented in idt.c */
void set_idt_gate(uint8_t n, uint64_t handler);
void load_idt();

#endif