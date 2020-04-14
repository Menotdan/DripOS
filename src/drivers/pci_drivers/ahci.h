#ifndef PCI_AHCI_H
#define PCI_AHCI_H
#include <stdint.h>
#include "drivers/pci.h"


typedef volatile struct {
    uint32_t command_list_base;
    uint32_t command_list_base_upper;
    uint32_t fis_base;
    uint32_t fis_base_upper;
    uint32_t interrupt_status;
    uint32_t interrupt_enable;
    uint32_t command;
    uint32_t reserved0;
    uint32_t task_file;
    uint32_t signature;
    uint32_t sata_status;
    uint32_t sata_control;
    uint32_t sata_error;
    uint32_t sata_active;
    uint32_t command_issued;
    uint32_t sata_notif;
    uint32_t fis_base_switch;

    uint32_t reserved1[11];
    uint32_t vendor[4];
} __attribute__((packed)) ahci_port_t;

typedef struct {
    ahci_port_t *port;
    uint8_t addresses_64;
    uint8_t command_slot_count;
    uint64_t sector_size;
    uint64_t sector_count;
    uint8_t lba48;
} ahci_port_data_t;

typedef struct {
    uint32_t data_base;
    uint32_t data_base_upper;
    uint32_t reserved0;

    uint32_t byte_count:22;
    uint32_t reserved1:9;
    uint32_t interrupt:1;
} __attribute__((packed)) ahci_prdt_entry_t;

typedef struct {
    uint8_t command_fis_data[64];
    uint8_t atapi_command[16];
    uint8_t reserved[48];

    ahci_prdt_entry_t prdts[];
} __attribute__((packed)) ahci_command_entry_t;


#define AHCI_GET_FIS_SIZE(n_fis) (sizeof(ahci_command_entry_t) + (sizeof(ahci_prdt_entry_t) * n_fis))
#define AHCI_GET_PRDT_BYTES(count) ((((count + 1) & ~1) - 1) & 0x3FFFFF)

void ahci_init_controller(pci_device_t device);
void ahci_identify_sata(ahci_port_data_t *port, uint8_t packet_interface);
int ahci_io_sata_sectors(ahci_port_data_t *port, void *buf, uint16_t count, uint64_t offset, uint8_t write);
int ahci_read_sata_bytes(ahci_port_data_t *port, void *buf, uint64_t count, uint64_t seek);
int ahci_write_sata_bytes(ahci_port_data_t *port, void *buf, uint64_t count, uint64_t seek);

#endif