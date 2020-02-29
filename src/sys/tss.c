#include "tss.h"
#include "sys/smp.h"
#include "klibc/stdlib.h"

extern char GDT64[];

void load_tss() {
    uint64_t tss_pointer = (uint64_t) &(get_cpu_locals()->tss);
    uint64_t GDT_PTR = (uint64_t) GDT64 + TSS_GDT_OFFSET;
    tss_64_descriptor_t *tss_descriptor = (tss_64_descriptor_t *) GDT_PTR;
    
    /* Actually set the data */
    tss_descriptor->segment_base_low = (uint32_t) ((tss_pointer >> 0) & 0xffff);
    tss_descriptor->segment_base_mid = (uint32_t) ((tss_pointer >> 16) & 0xff);
    tss_descriptor->segment_base_mid2 = (uint32_t) ((tss_pointer >> 24) & 0xff);
    tss_descriptor->segment_base_high = (uint32_t) ((tss_pointer >> 32) & 0xffffffff);
    
    tss_descriptor->segment_limit_low = 0x67;
    tss_descriptor->segment_present = 1;
    tss_descriptor->segment_type = 0b1001;
    
    asm volatile("ltr %%ax" :: "a"(TSS_GDT_OFFSET));
}

void set_kernel_stack(uint64_t new_stack_addr) {
    get_cpu_locals()->tss.ist_stack1 = new_stack_addr;
    get_cpu_locals()->tss.rsp0 = new_stack_addr;
}

void set_panic_stack(uint64_t panic_stack_addr) {
    get_cpu_locals()->tss.ist_stack2 = panic_stack_addr;
}