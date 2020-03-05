#ifndef PCI_AHCI_H
#define PCI_AHCI_H
#include <stdint.h>
#include "drivers/pci.h"

void ahci_init_controller(pci_device_t device);

#endif