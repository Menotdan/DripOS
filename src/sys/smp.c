#include "smp.h"
#include "mm/vmm.h"
#include "io/msr.h"
#include "sys/apic.h"
#include "klibc/stdlib.h"
#include "klibc/string.h"
#include "klibc/hashmap.h"
#include "drivers/pit.h"
#include "drivers/serial.h"
#include "drivers/tty/tty.h"

extern char smp_trampoline[];
extern char smp_trampoline_end[];
extern char GDT_PTR_32[];
extern char GDT_PTR_64[];
extern char GDT64[];
extern char long_smp_loaded[];

uint8_t cores_booted = 1;

hashmap_t *cpu_locals_list = (void *) 0;

void new_cpu_locals() {
    if (!cpu_locals_list) {
        cpu_locals_list = init_hashmap();
    }
    cpu_locals_t *new_locals = kcalloc(sizeof(cpu_locals_t));
    new_locals->meta_pointer = (uint64_t) new_locals;
    write_msr(0xC0000101, (uint64_t) new_locals);
}

cpu_locals_t *get_cpu_locals() {
    cpu_locals_t *ret;
    asm volatile("movq %%gs:(0), %0;" : "=r"(ret));
    return ret;
}

uint8_t get_lapic_id() {
    return (read_lapic(0x20) >> 24) & 0xFF;
}

void send_ipi(uint8_t ap, uint32_t ipi_number) {
    write_lapic(0x310, (ap << 24));
    write_lapic(0x300, ipi_number);
}

static void write_cpu_data32(uint16_t offset, uint32_t data) {
    *(volatile uint32_t *) (0x500 + NORMAL_VMA_OFFSET + offset) = data;
}

static void write_cpu_data64(uint16_t offset, uint64_t data) {
    *(volatile uint64_t *) (0x500 + NORMAL_VMA_OFFSET + offset) = data;
}

void launch_cpus() {
    /* Copy trampoline code */
    uint64_t code_size = smp_trampoline_end - smp_trampoline;
    memcpy((uint8_t *) smp_trampoline, (uint8_t *) (0x1000 + NORMAL_VMA_OFFSET), code_size);
    memset((uint8_t *) (0x500 + NORMAL_VMA_OFFSET), 0, 0xB00);

    vmm_remap((void *) 0, (void *) 0, 1 + ((code_size + 0x1000 - 1) / 0x1000), VMM_WRITE | VMM_PRESENT);
    vmm_map((void *) (GDT64 - KERNEL_VMA_OFFSET), (void *) (GDT64 - KERNEL_VMA_OFFSET), 1, VMM_WRITE | VMM_PRESENT);

    /* Setup all the CPUs */
    madt_ent0_t *cpu;
    for (uint64_t i = 0; i < cpu_vector.items_count; i++) {
        cpu = cpu_vector.items[i];
        if (cpu->apic_id != get_lapic_id()) {
            if (cpu->cpu_flags & 0x1 || cpu->cpu_flags & 0x2) {
                /* Clear flags */
                memset((uint8_t *) (0x500 + NORMAL_VMA_OFFSET), 0, 0xB00);

                write_cpu_data32(0x10, (uint32_t) vmm_get_pml4t()); // Set the cr3
                memcpy((uint8_t *) GDT_PTR_32, (uint8_t *) (0x520 + NORMAL_VMA_OFFSET), 6); // Copy the GDT pointer
                memcpy((uint8_t *) GDT_PTR_64, (uint8_t *) (0x530 + NORMAL_VMA_OFFSET), 10); // Copy the 64 bit GDT pointer
                write_cpu_data64(0x40, (uint64_t) kmalloc(0x4000) + 0x4000);
                write_cpu_data64(0x50, (uint64_t) long_smp_loaded);

                *(volatile uint8_t *) (0x560 + NORMAL_VMA_OFFSET) = i;

                send_ipi(cpu->apic_id, 0x500);
                sleep_no_task(10);
                send_ipi(cpu->apic_id, 0x600 | 1);
                sleep_no_task(10);
                if (*GET_HIGHER_HALF(uint16_t *, 0x500) == 1 
                    || *GET_HIGHER_HALF(uint16_t *, 0x500) == 2) {
                    kprintf("Cpu %lu booted\n", (uint64_t) ++cores_booted);
                    while (*GET_HIGHER_HALF(uint16_t *, 0x500) != 2) { asm volatile("nop"); }
                } else {
                    send_ipi(cpu->apic_id, 0x500);
                    sleep_no_task(10);
                    send_ipi(cpu->apic_id, 0x600 | 1);
                    sleep_no_task(1000);
                    if (*GET_HIGHER_HALF(uint16_t *, 0x500) == 1 
                        || *GET_HIGHER_HALF(uint16_t *, 0x500) == 2) {
                        kprintf("Cpu %lu booted\n", (uint64_t) ++cores_booted);
                        while (*GET_HIGHER_HALF(uint16_t *, 0x500) != 2) { asm volatile("nop"); }
                    } else {
                        kprintf("Failed CPU boot :(\n");
                    }
                }
            }
        }
    }
    vmm_unmap((void *) 0, 1 + ((code_size + 0x1000 - 1) / 0x1000));
    vmm_unmap((void *) (GDT64 - KERNEL_VMA_OFFSET), 1);
}

void smp_entry_point() {
    kprintf("Hello from SMP\n");

    new_cpu_locals(); // Setup CPU locals for this CPU
    cpu_locals_t *cpu_locals = get_cpu_locals();
    cpu_locals->apic_id = get_lapic_id();
    cpu_locals->cpu_index = *(uint8_t *) (0x560 + NORMAL_VMA_OFFSET);
    hashmap_set_elem(cpu_locals_list, get_cpu_index(), cpu_locals);

    load_tss();
    set_panic_stack((uint64_t) kmalloc(0x1000) + 0x1000);
    set_kernel_stack((uint64_t) kmalloc(0x1000) + 0x1000);
    kprintf("Our index is %u\n", (uint32_t) cpu_locals->cpu_index);
    configure_apic_ap();
    scheduler_init_ap();
    configure_idt();

    /* After init, let the BSP know that we are done */
    *GET_HIGHER_HALF(uint16_t *, 0x500) = 2;
    while (1) { asm volatile("hlt"); }
}