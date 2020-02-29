#ifndef TSS_H
#define TSS_H
#include <stdint.h>

#define TSS_GDT_OFFSET 0x20

typedef struct {
    uint32_t reserved;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;


    uint32_t reserved_1;
    uint32_t reserved_2;

    uint64_t ist_stack1;
    uint64_t ist_stack2;
    uint64_t ist_stack3;
    uint64_t ist_stack4;
    uint64_t ist_stack5;
    uint64_t ist_stack6;
    uint64_t ist_stack7;

    uint64_t reserved_3;
    uint16_t reserved_4;
    uint16_t io_bitmap_offset;
} __attribute__((packed)) tss_64_t;

typedef struct {
    uint32_t segment_limit_low  : 16;
    uint32_t segment_base_low   : 16;
    uint32_t segment_base_mid   : 8;
    uint32_t segment_type       : 4;
    uint32_t zero_1             : 1;
    uint32_t segment_dpl        : 2;
    uint32_t segment_present    : 1;
    uint32_t segment_limit_high : 4;
    uint32_t segment_avail      : 1;
    uint32_t zero_2             : 2;
    uint32_t segment_gran       : 1;
    uint32_t segment_base_mid2  : 8;
    uint32_t segment_base_high  : 32;
    uint32_t reserved_1         : 8;
    uint32_t zero_3             : 5;
    uint32_t reserved_2         : 19;
} __attribute__((packed)) tss_64_descriptor_t;

void load_tss();

#endif