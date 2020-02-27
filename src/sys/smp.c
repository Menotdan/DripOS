#include "smp.h"
#include "mm/vmm.h"
#include "sys/apic.h"
#include "klibc/stdlib.h"
#include "klibc/string.h"
#include "drivers/serial.h"
#include "drivers/tty/tty.h"

extern char smp_trampoline[];
extern char smp_trampoline_end[];
extern char GDT_PTR_32[];
extern char GDT_PTR_64[];
extern char GDT64[];
extern char long_smp_loaded[];

uint8_t cores_booted = 1;

uint8_t get_lapic_id() {
    return (read_lapic(0x20) >> 24) & 0xFF;
}

void send_ipi(uint8_t ap, uint32_t ipi_number) {
    write_lapic(0x310, (ap << 24));
    write_lapic(0x300, ipi_number);
}

void write_cpu_data8(uint16_t offset, uint8_t data) {
    *(uint8_t *) (0x500 + 0xFFFF800000000000 + offset) = data;
}

void write_cpu_data16(uint16_t offset, uint16_t data) {
    *(uint16_t *) (0x500 + 0xFFFF800000000000 + offset) = data;
}

void write_cpu_data32(uint16_t offset, uint32_t data) {
    *(uint32_t *) (0x500 + 0xFFFF800000000000 + offset) = data;
}

void write_cpu_data64(uint16_t offset, uint64_t data) {
    *(uint64_t *) (0x500 + 0xFFFF800000000000 + offset) = data;
}

void launch_cpus() {
    /* Copy trampoline code */
    sprintf("\nCopying trampoline code");
    uint64_t code_size = smp_trampoline_end - smp_trampoline;
    memcpy((uint8_t *) smp_trampoline, (uint8_t *) (0x1000 + 0xFFFF800000000000), code_size);
    memset((uint8_t *) (0x500 + 0xFFFF800000000000), 0, 0xB00);

    vmm_map((void *) 0, (void *) 0, 1 + ((code_size + 0x1000 - 1) / 0x1000), VMM_WRITE | VMM_PRESENT);
    vmm_map((void *) (GDT64 - 0xFFFFFFFF80000000), (void *) (GDT64 - 0xFFFFFFFF80000000), 1, VMM_WRITE | VMM_PRESENT);

    sprintf("\nDone copying");
    /* Setup all the CPUs */
    madt_ent0_t *cpu;
    for (uint64_t i = 0; i < cpu_vector.items_count; i++) {
        cpu = cpu_vector.items[i];
        if (cpu->apic_id != get_lapic_id()) {
            sprintf("\nFound AP");
            if (cpu->cpu_flags & 0x1 || cpu->cpu_flags & 0x2) {
                sprintf("\nFound bootable AP");

                /* Clear flags */
                memset((uint8_t *) (0x500 + 0xFFFF800000000000), 0, 0xB00);

                write_cpu_data32(0x10, (uint32_t) vmm_get_pml4t()); // Set the cr3
                memcpy((uint8_t *) GDT_PTR_32, (uint8_t *) (0x520 + 0xFFFF800000000000), 6); // Copy the GDT pointer
                memcpy((uint8_t *) GDT_PTR_64, (uint8_t *) (0x530 + 0xFFFF800000000000), 10); // Copy the 64 bit GDT pointer
                write_cpu_data64(0x40, (uint64_t) kmalloc(0x4000));
                write_cpu_data64(0x50, (uint64_t) long_smp_loaded);
                sprintf("\nAddr: %lx", *(uint64_t *) (0x500 + 0xFFFF800000000000 + 0x50));

                send_ipi(cpu->apic_id, 0x500);
                sleep_no_task(10);
                send_ipi(cpu->apic_id, 0x600 | 1);
                sleep_no_task(10);
                if (*(uint16_t *) (0x500 + 0xFFFF800000000000) == 1) {
                    sprintf("\nCpu %lu booted", (uint64_t) ++cores_booted);
                } else {
                    send_ipi(cpu->apic_id, 0x500);
                    sleep_no_task(10);
                    send_ipi(cpu->apic_id, 0x600 | 1);
                    sleep_no_task(1000);
                    if (*(uint16_t *) (0x500 + 0xFFFF800000000000) == 1) {
                        sprintf("\nCpu %lu booted", (uint64_t) ++cores_booted);
                    } else {
                        sprintf("\nNo boot :(");
                    }
                }
            }
        } else {
            sprintf("\nFound BSP");
        }
    }
    vmm_unmap((void *) 0, 1 + ((code_size + 0x1000 - 1) / 0x1000));
    vmm_unmap((void *) (GDT64 - 0xFFFFFFFF80000000), 1);
}

uint8_t get_cpu_count() {
    return cores_booted;
}

void smp_entry_point() {
    kprintf("\nHello from SMP");
    while (1) { asm volatile("hlt"); }
}