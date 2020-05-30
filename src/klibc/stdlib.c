#include "stdlib.h"
#include "string.h"
#include "mm/pmm.h"
#include "mm/vmm.h"
#include "proc/scheduler.h"
#include "klibc/lock.h"

#include "drivers/tty/tty.h"
#include "drivers/serial.h"

void *kmalloc(uint64_t size) {
    uint64_t size_data = (uint64_t) pmm_alloc(size + 0x2000) + NORMAL_VMA_OFFSET;
    uint64_t page_count = ((size + 0x2000) + 0x1000 - 1) / 0x1000;
    uint64_t last_page = size_data + (page_count * 0x1000) - 1;

    /* Unmap top for high bounds reads/writes */
    vmm_unmap((void *) last_page, 1);
    *(uint64_t *) size_data = size + 0x2000;

    /* Unmap size data for lower bounds reads/writes */
    //sprintf("+mem %lu %lu\n", size_data, *(uint64_t *) size_data);
    vmm_unmap((void *) size_data, 1);
    return (void *) (size_data + 0x1000);
}

void kfree(void *addr) {
    /* Map size */
    vmm_map(GET_LOWER_HALF(void *, (uint64_t) addr - 0x1000), (void *) ((uint64_t) addr - 0x1000), 1, VMM_PRESENT | VMM_WRITE);
    uint64_t *size_data = (uint64_t *) ((uint64_t) addr - 0x1000);
    uint64_t page_count = ((*size_data - 0x1000) + 0x1000 - 1) / 0x1000;
    uint64_t last_page = (uint64_t) addr + (page_count * 0x1000) - 1;

    /* Map last page */
    vmm_map(GET_LOWER_HALF(void *, last_page), (void *) last_page, 1, VMM_PRESENT | VMM_WRITE);
    void *phys = virt_to_phys((void *) size_data, (pt_t *) vmm_get_pml4t());
    if ((uint64_t) phys != 0xFFFFFFFFFFFFFFFF) {
        pmm_unalloc(phys, *size_data);
    }
    //sprintf("-mem %lu %lu\n", size_data, *size_data);
}

void *kcalloc(uint64_t size) {
    void *buffer = kmalloc(size);
    memset((uint8_t *) buffer, 0, size);
    return buffer;
}

void *krealloc(void *addr, uint64_t new_size) {
    vmm_map(GET_LOWER_HALF(void *, (uint64_t) addr - 0x1000), (void *) ((uint64_t) addr - 0x1000), 1, VMM_PRESENT | VMM_WRITE);
    uint64_t *size_data = (uint64_t *) ((uint64_t) addr - 0x1000);
    void *new_buffer = kcalloc(new_size);
    
    if (!addr) return new_buffer;

    /* Copy everything over, and only copy part if our new size is lower than the old size */
    memcpy((uint8_t *) addr, (uint8_t *) new_buffer, (*size_data) - 0x2000);

    kfree(addr);
    return new_buffer;
}

void panic(char *msg) {
    if (check_interrupts()) {
        asm volatile("mov %0, %%rdi; int $251;" ::"r"(msg) : "memory");
    } else {
        kprintf("Panic! Message: %s\n", msg);
    }

    // We shouldnt return
    while (1) { asm volatile("hlt"); }
}