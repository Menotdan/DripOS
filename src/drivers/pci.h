#ifndef PCI_H
#define PCI_H
#include <stdint.h>

typedef struct {
    uint8_t bus;
    uint8_t device;
    uint8_t function;
} pci_device_t;

typedef struct {
    uint8_t class;
    uint8_t subclass;
    uint8_t prog_if;
} pci_id_t;

void pci_init();

#endif