#include "stdlib.h"
#include "string.h"
#include "mm/pmm.h"
#include "mm/vmm.h"
#include "proc/scheduler.h"
#include "klibc/lock.h"
#include "drivers/pit.h"

void *kmalloc(uint64_t size) {
    uint64_t size_data = (uint64_t) pmm_alloc(size + 0x1000) + NORMAL_VMA_OFFSET;
    *(uint64_t *) size_data = size + 0x1000;
    return (void *) (size_data + 0x1000);
}

void kfree(void *addr) {
    uint64_t *size_data = (uint64_t *) ((uint64_t) addr - 0x1000);
    void *phys = virt_to_phys((void *) size_data, (pt_t *) vmm_get_pml4t());
    if ((uint64_t) phys != 0xFFFFFFFFFFFFFFFF) {
        pmm_unalloc(phys, *size_data);
    }
}

void *kcalloc(uint64_t size) {
    void *buffer = kmalloc(size);
    memset((uint8_t *) buffer, 0, size);
    return buffer;
}

void *krealloc(void *addr, uint64_t new_size) {
    uint64_t *size_data = (uint64_t *) ((uint64_t) addr - 0x1000);
    void *new_buffer = kcalloc(new_size);
    
    if (!addr) return new_buffer;

    /* Copy everything over, and only copy part if our new size is lower than the old size */
    memcpy((uint8_t *) addr, (uint8_t *) new_buffer, (*size_data) - 0x1000);

    kfree(addr);
    return new_buffer;
}

void yield() {
    if (scheduler_enabled && check_interrupts()) {
        asm volatile("int $254");
    }
}

/* Inefficient, taskless sleep */
void sleep_no_task(uint64_t milliseconds) {
    uint64_t start = global_ticks;
    while (global_ticks < start + milliseconds) { continue; }
}