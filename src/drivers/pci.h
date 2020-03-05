#ifndef PCI_H
#define PCI_H
#include <stdint.h>

typedef struct {
    uint8_t valid; // For get_pci_device
    uint8_t bus;
    uint8_t device;
    uint8_t function;
} pci_device_t;

typedef struct {
    uint8_t class;
    uint8_t subclass;
    uint8_t prog_if;
    uint16_t device_id;
    uint16_t vendor_id;
} pci_id_t;

typedef void (*pci_driver_t)(pci_device_t);

void pci_init();
uint32_t pci_get_bar(pci_device_t device, uint8_t bar);
uint32_t pci_get_bar_size(pci_device_t device, uint8_t bar);
pci_id_t get_pci_ids(uint8_t bus, uint8_t device, uint8_t function);
pci_device_t get_pci_device(uint64_t index);
uint64_t pci_device_count();

#endif