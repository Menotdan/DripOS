#include "stdlib.h"
#include "string.h"
#include "mm/pmm.h"
#include "mm/vmm.h"
#include "proc/scheduler.h"
#include "klibc/lock.h"
#include "klibc/logger.h"

#include "drivers/tty/tty.h"
#include "drivers/serial.h"

void *kmalloc(uint64_t size) {
    interrupt_state_t state = interrupt_lock();
    uint64_t size_data = (uint64_t) pmm_alloc(size + 0x2000) + NORMAL_VMA_OFFSET;
    uint64_t page_count = ((size + 0x2000) + 0x1000 - 1) / 0x1000;
    uint64_t last_page = size_data + ((page_count - 1) * 0x1000);

    /* Unmap top for high OOB reads/writes */
    vmm_unmap((void *) last_page, 1);
    *(uint64_t *) size_data = size + 0x2000;
    *(uint64_t *) (size_data + 8) = MALLOC_SIGNATURE;

    log_alloc("+mem %lu %lu %lx\n", size_data, *(uint64_t *) size_data, __builtin_return_address(0));

    /* Unmap size data for low OOB reads/writes */
    vmm_unmap((void *) size_data, 1);
    interrupt_unlock(state);

    if (size_data % 0x1000 != 0) {
        error("{-kmalloc-} died, caller: %lx, size: %lu\n", __builtin_return_address(0), size);
        assert(!"kmalloc bug");
    }
    return (void *) (size_data + 0x1000);
}

//void *cur_pain = (void *) 0;
// 727
void kfree(void *addr) {
    if (((uint64_t) addr % 0x1000) != 0) {
        warn("{-kfree-} unaligned addr: %lx, caller: %lx", addr, __builtin_return_address(0));
    }

    assert(((uint64_t) addr % 0x1000) == 0); // is divisible by page size
    assert(is_mapped(addr));
    interrupt_state_t state = interrupt_lock();

    /* Map size */
    vmm_map(GET_LOWER_HALF(void *, (uint64_t) addr - 0x1000), (void *) ((uint64_t) addr - 0x1000), 1, VMM_PRESENT | VMM_WRITE);
    uint64_t *size_data = (uint64_t *) ((uint64_t) addr - 0x1000);
    uint64_t *signature = (uint64_t *) ((uint64_t) addr - 0x0ff8);
    uint64_t page_count = ((*size_data - 0x1000) + 0x1000 - 1) / 0x1000;
    uint64_t last_page = (uint64_t) addr + ((page_count - 1) * 0x1000);

    /* Map last page */
    vmm_map(GET_LOWER_HALF(void *, last_page), (void *) last_page, 1, VMM_PRESENT | VMM_WRITE);
    if (*signature != MALLOC_SIGNATURE) {
        kprintf("signature check failed in kfree()! addr: %lx, caller: %lx\n", addr, __builtin_return_address(0));
        while (1) {
            assert(!"h");
        }
    }

    void *phys = virt_to_phys((void *) size_data, (page_table_t *) vmm_get_base());
    if ((uint64_t) phys != 0xFFFFFFFFFFFFFFFF) {
        log_alloc("-mem %lu %lu %lx\n", size_data, *size_data, __builtin_return_address(0));
        pmm_unalloc(phys, *size_data);
    }

    interrupt_unlock(state);
}

void unmap_alloc(void *addr) {
    interrupt_state_t state = interrupt_lock();

    /* Map size */
    vmm_map(GET_LOWER_HALF(void *, (uint64_t) addr - 0x1000), (void *) ((uint64_t) addr - 0x1000), 1, VMM_PRESENT | VMM_WRITE);
    uint64_t *size_data = (uint64_t *) ((uint64_t) addr - 0x1000);
    uint64_t page_count = ((*size_data - 0x1000) + 0x1000 - 1) / 0x1000;

    vmm_unmap(size_data, page_count);

    interrupt_unlock(state);
}

void remap_alloc(void *addr) {
    interrupt_state_t state = interrupt_lock();

    /* Map size */
    vmm_map(GET_LOWER_HALF(void *, (uint64_t) addr - 0x1000), (void *) ((uint64_t) addr - 0x1000), 1, VMM_PRESENT | VMM_WRITE);
    uint64_t *size_data = (uint64_t *) ((uint64_t) addr - 0x1000);
    uint64_t page_count = ((*size_data - 0x1000) + 0x1000 - 1) / 0x1000;

    vmm_map(GET_LOWER_HALF(void *, addr), (void *) ((uint64_t) addr), page_count - 1, VMM_PRESENT | VMM_WRITE);

    interrupt_unlock(state);
}

void *kcalloc(uint64_t size) {
    interrupt_state_t state = interrupt_lock();
    void *buffer = kmalloc(size);

    memset((uint8_t *) buffer, 0, size);
    interrupt_unlock(state);
    return buffer;
}

void *krealloc(void *addr, uint64_t new_size) {
    interrupt_state_t state = interrupt_lock();
    void *new_buffer = kcalloc(new_size);
    if (!addr) { interrupt_unlock(state); return new_buffer; }

    vmm_map(GET_LOWER_HALF(void *, (uint64_t) addr - 0x1000), (void *) ((uint64_t) addr - 0x1000), 1, VMM_PRESENT | VMM_WRITE);
    uint64_t *size_data = (uint64_t *) ((uint64_t) addr - 0x1000);

    /* Copy everything over, and only copy part if our new size is lower than the old size */
    memcpy((uint8_t *) addr, (uint8_t *) new_buffer, (*size_data) - 0x2000);

    kfree(addr);
    interrupt_unlock(state);
    return new_buffer;
}

void panic(char *msg) {
    if (check_interrupts()) {
        sprintf("Panic Message Address: %lx\n", msg);
        sprintf("Panic Message: %s\n", msg);
        asm volatile("mov %0, %%rdi; int $251;" ::"r"(msg) : "memory");
    } else {
        kprintf("Panic! Message: %s\n", msg);
    }

    // We shouldnt return
    while (1) { asm volatile("hlt"); }
}

char *normalize_path(char *path) {
    char *normalized = kcalloc(4097);
    if (path[0] != '/') {
        strcat(normalized, "/");
    }

    strcat(normalized, path);
    if (normalized[strlen(normalized) - 1] != '/') {
        strcat(normalized, "/");
    }

    return normalized;
}