#ifndef DRIPDBG_PROTOCOL_H
#define DRIPDBG_PROTOCOL_H
#include <stdint.h>

typedef struct {
    uint8_t type;
    uint64_t total_data;
    uint8_t data[];
} __attribute__((packed)) packet_t;

#endif