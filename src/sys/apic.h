#ifndef APIC_H
#define APIC_H
#include <stdint.h>
#include "sys/acpi/rsdt.h"

#define APIC_BASE_MSR 0x1B
#define APIC_BASE_MSR_ENABLE 0x800
#define KERNEL_VM_OFFSET 0xFFFF800000000000

typedef struct {
    uint8_t acpi_processor_id;
    uint8_t apic_id;
    uint32_t cpu_flags;
} __attribute__ ((packed)) madt_ent0_t;

typedef struct {
    uint8_t ioapic_id;
    uint8_t reserved;
    uint32_t ioapic_addr;
    uint32_t gsi_base;
} __attribute__ ((packed)) madt_ent1_t;

typedef struct {
    uint8_t bus_src;
    uint8_t irq_src;
    uint32_t gsi;
    uint16_t flags;
} __attribute__ ((packed)) madt_ent2_t;

typedef struct {
    uint8_t acpi_processor_id;
    uint16_t flags;
    uint8_t lint;
} __attribute__ ((packed)) madt_ent4_t;

typedef struct {
    uint16_t reserved;
    uint64_t local_apic_override;
} __attribute__ ((packed)) madt_ent5_t;

typedef struct {
    sdt_header_t header; // The header
    /* Data for the MADT */
    uint32_t local_apic_addr;
    uint32_t apic_flags;
    uint8_t entries[];
} __attribute__ ((packed)) madt_t;

void parse_madt();
void configure_apic();

#endif