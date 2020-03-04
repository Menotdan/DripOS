#ifndef IDT_H
#define IDT_H
#include <stdint.h>

/* Segment selectors */
#define KERNEL_CS 0x08
#define KERNEL_DS 0x10
#define USER_CS 27
#define USER_DS 0x20

/* How every interrupt gate (handler) is defined */
typedef struct {
    uint16_t low_offset;
    uint16_t selector;
    uint8_t ist;
    uint8_t type;
    uint16_t middle_offset;
    uint32_t high_offset;
    uint32_t always0;
} __attribute__((packed)) idt_gate_t;

/* A pointer to the array of interrupt handlers.
 * Assembly instruction 'lidt' will read it */
typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) idt_register_t;

#define IDT_ENTRIES 256
idt_register_t idt_reg;

/* Functions implemented in idt.c */
void set_idt_gate(uint8_t n, uint64_t handler);
void set_ist(uint8_t n, uint8_t ist_index);
void load_idt();

#endif