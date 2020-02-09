#include <stdint.h>
#include <stddef.h>
#include "klibc/string.h"
#include "drivers/serial.h"
#include "mm/pmm.h"
#include "mm/vmm.h"
#include "multiboot.h"

pt_off_t k_virt_to_offs(void *virt) {
    uintptr_t addr = (uintptr_t)virt;

    pt_off_t off = {
        .p4_off = (addr & ((size_t)0x1ff << 39)) >> 39,
        .p3_off = (addr & ((size_t)0x1ff << 30)) >> 30,
        .p2_off = (addr & ((size_t)0x1ff << 21)) >> 21,
        .p1_off = (addr & ((size_t)0x1ff << 12)) >> 12,
    };

    return off;
}

// Kernel main function, execution starts here :D
void kmain(multiboot_info_t *mboot_dat) {
    if (mboot_dat) {
        pmm_memory_setup(mboot_dat);
    }

    sprintf("\nTest alloc: %lx", pmm_alloc(0x2000));
    sprintf("\nTest alloc 2: %lx", pmm_alloc(1));

    int test = vmm_map((void *) 0, (void *) 0x10000000, 1000, VMM_WRITE);
    pt_off_t offs = k_virt_to_offs((void *) 0x10000000);
    sprintf("\nP4: %lu P3: %lu P2: %lu P1: %lu", offs.p4_off, offs.p3_off, offs.p2_off, offs.p1_off);
    sprintf("\n%lx", *(uint64_t*) 0x10000000);
    sprintf("\nVMM return: %u", test);

    while (1) {
        asm volatile("hlt");
    }
}