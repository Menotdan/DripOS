#ifndef VMM_H
#define VMM_H
#include <stdint.h>

#define VMM_4K_PERM_MASK ~(0xfff)
#define VMM_2M_PERM_MASK ~(0x1fffff)

#define VMM_PRESENT (1<<0)
#define VMM_WRITE (1<<1)
#define VMM_USER (1<<2)
#define VMM_WRITE_THROUGH (1<<3)
#define VMM_NO_CACHE (1<<4)
#define VMM_ACCESS (1<<5)
#define VMM_DIRTY (1<<6)
#define VMM_HUGE (1<<7)

#define NORMAL_VMA_OFFSET 0xFFFF800000000000
#define KERNEL_VMA_OFFSET 0xFFFFFFFF80000000

#define GET_HIGHER_HALF(type, lower_half) (type) ((uint64_t) lower_half + NORMAL_VMA_OFFSET)
#define GET_LOWER_HALF(type, higher_half) (type) ((uint64_t) higher_half - NORMAL_VMA_OFFSET)

typedef struct {
    uint64_t p4_off;
    uint64_t p3_off;
    uint64_t p2_off;
    uint64_t p1_off;
} pt_off_t;

typedef struct {
    uint64_t table[512];
} pt_t;

typedef struct {
    pt_t *p4;
    pt_t *p3;
    pt_t *p2;
    pt_t *p1;
} pt_ptr_t;

extern uint64_t base_kernel_cr3;

int vmm_map(void *phys, void *virt, uint64_t count, uint16_t perms);
int vmm_remap(void *phys, void *virt, uint64_t count, uint16_t perms);
int vmm_unmap(void *virt, uint64_t count);
void vmm_set_pat(void *virt, uint64_t count, uint8_t pat_entry);
int vmm_map_pages(void *phys, void *virt, void *p4, uint64_t count, uint16_t perms);
int vmm_remap_pages(void *phys, void *virt, void *p4, uint64_t count, uint16_t perms);
int vmm_unmap_pages(void *virt, void *p4, uint64_t count);
void vmm_set_pat_pages(void *virt, void *p4, uint64_t count, uint8_t pat_entry);
void vmm_set_pml4t(uint64_t new);
void *virt_to_phys(void *virt, pt_t *p4);
uint8_t is_mapped(void *data);
uint8_t range_mapped(void *data, uint64_t size);
uint64_t vmm_get_pml4t();

void *vmm_fork(void *old);
void *vmm_fork_higher_half(void *old);
void *vmm_fork_lower_half(void *old);

uint64_t get_entry(pt_t *cur_table, uint64_t offset);

#endif