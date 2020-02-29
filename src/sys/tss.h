#ifndef TSS_H
#define TSS_H
#include <stdint.h>

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
} tss_64_t;
#endif