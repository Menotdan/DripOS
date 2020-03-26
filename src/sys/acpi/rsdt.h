#ifndef RSDT_H
#define RSDT_H
#include <stdint.h>

#define VIRTUAL_EBDA_PTR 0xFFFF80000000040E

typedef struct {
    char signature[8];
    uint8_t checksum;
    char OEMID[6];
    uint8_t revision;
    uint32_t rsdt_address;
} __attribute__ ((packed)) rsdp1_t;

typedef struct {
    rsdp1_t old_descriptor;

    uint32_t length;
    uint64_t xsdt_address;
    uint8_t extendedChecksum;
    uint8_t reserved[3];
} __attribute__ ((packed)) rsdp2_t;

typedef struct {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char OEMID[6];
    char OEM_table_ID[8];
    uint32_t OEM_revision;
    uint32_t creator_ID;
    uint32_t creator_revision;
} __attribute__ ((packed)) sdt_header_t;

typedef struct {
    sdt_header_t header;
    uint32_t other_sdts[];
} __attribute__ ((packed)) rsdt_t;

typedef struct {
    sdt_header_t header;
    uint64_t other_sdts[];
} __attribute__ ((packed)) xsdt_t;


uint64_t get_ebda();
rsdp1_t *get_rsdp1(uint64_t ebda);
sdt_header_t *get_root_sdt_header();
sdt_header_t *search_sdt_header(char *sig);

#endif