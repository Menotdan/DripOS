#include "apic.h"
#include "io/ports.h"
#include "mm/vmm.h"
#include "klibc/vector.h"
#include "drivers/serial.h"

uint64_t lapic_base;
vector_t cpu_vector;
vector_t iso_vector;
vector_t ioapic_vector;
vector_t nmi_vector;

void parse_madt() {
    madt_t *madt = (madt_t *) search_sdt_header("APIC");

    if (madt) {
        /* Initialize all of our vectors */
        vector_init(&cpu_vector);
        vector_init(&iso_vector);
        vector_init(&ioapic_vector);
        vector_init(&nmi_vector);
        uint64_t bytes_for_entries = madt->header.length - (sizeof(madt->header) + sizeof(madt->local_apic_addr) + sizeof(madt->apic_flags));
        lapic_base = (uint64_t) madt->local_apic_addr;

        for (uint64_t e = 0; e < bytes_for_entries; e++) {
            uint8_t type = madt->entries[e++];
            uint8_t size = madt->entries[e++];

            if (type == 0) {
                madt_ent0_t *ent = (madt_ent0_t *) &(madt->entries[e]);
                sprintf("\nCPU:\n");
                sprintf("  ACPI ID: %u\n", (uint32_t) ent->acpi_processor_id);
                sprintf("  APIC ID: %u\n", (uint32_t) ent->apic_id);
                sprintf("  Flags: %u", ent->cpu_flags);
                vector_add(&cpu_vector, (void *) ent);
            } else if (type == 1) {
                madt_ent1_t *ent = (madt_ent1_t *) &(madt->entries[e]);
                sprintf("\nIOAPIC:\n");
                sprintf("  ID: %u\n", (uint32_t) ent->ioapic_id);
                sprintf("  Addr: %x\n", (uint32_t) ent->ioapic_addr);
                sprintf("  GSI Base: %x", (uint32_t) ent->gsi_base);
                vector_add(&ioapic_vector, (void *) ent);
            } else if (type == 2) {
                madt_ent2_t *ent = (madt_ent2_t *) &(madt->entries[e]);
                sprintf("\nISO:\n");
                sprintf("  Bus src: %u\n", (uint32_t) ent->bus_src);
                sprintf("  IRQ src: %u\n", (uint32_t) ent->irq_src);
                sprintf("  GSI: %u\n", (uint32_t) ent->gsi);
                sprintf("  Flags: %x", (uint32_t) ent->flags);
                vector_add(&iso_vector, (void *) ent);
            } else if (type == 4) {
                madt_ent4_t *ent = (madt_ent4_t *) &(madt->entries[e]);
                sprintf("\nNMI:\n");
                sprintf("  Processor ID: %u\n", (uint32_t) ent->acpi_processor_id);
                sprintf("  LINT: %u\n", (uint32_t) ent->lint);
                sprintf("  Flags: %u", (uint32_t) ent->flags);
                vector_add(&nmi_vector, (void *) ent);
            } else if (type == 5) {
                madt_ent5_t *ent = (madt_ent5_t *) &(madt->entries[e]);
                sprintf("\nLAPIC override:\n");
                sprintf("  Addr: %lx", ent->local_apic_override);
                lapic_base = ent->local_apic_override;
            }

            e += size - 3;
        }
        sprintf("\nLAPIC addr: %lx", lapic_base);
        sprintf("\nCPUs: %lu", cpu_vector.items_count);
        sprintf("\nISOs: %lu", iso_vector.items_count);
        sprintf("\nIOAPICs: %lu", ioapic_vector.items_count);
        sprintf("\nNMIs: %lu", nmi_vector.items_count);
        /* Handle cross page mapping for the LAPIC */
        if (lapic_base / 0x1000 != (lapic_base + 0x400) / 0x1000) {
            vmm_map((void *) lapic_base, (void *) (lapic_base + 0xFFFF800000000000), 2, VMM_PRESENT | VMM_WRITE);
        } else {
            vmm_map((void *) lapic_base, (void *) (lapic_base + 0xFFFF800000000000), 1, VMM_PRESENT | VMM_WRITE);
        }
    }
}

void write_lapic(uint16_t offset, uint32_t data) {
    uint32_t volatile *lapic_register = (uint32_t volatile *) (lapic_base + offset);
    *lapic_register = data;
}

uint32_t read_lapic(uint16_t offset) {
    uint32_t volatile *lapic_register = (uint32_t volatile *) (lapic_base + offset);
    return *lapic_register;
}

void configure_lapic() {
    /* Remap the old PIC */
    port_outb(0x20, 0x11);
    port_outb(0xA0, 0x11);
    port_outb(0x21, 0x20);
    port_outb(0xA1, 0x28);
    port_outb(0x21, 0x04);
    port_outb(0xA1, 0x02);
    port_outb(0x21, 0x01);
    port_outb(0xA1, 0x01);
    port_outb(0x21, 0x0);
    port_outb(0xA1, 0x0);
    /* Disable the old PIC */
    port_outb(0xA1, 0xFF);
    port_outb(0x21, 0xFF);
    /* Now to configure the local APIC */

}