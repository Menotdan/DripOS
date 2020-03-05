#include "ahci.h"

#include "drivers/serial.h"
#include "drivers/tty/tty.h"

void ahci_init_controller(pci_device_t device) {
    pci_id_t ids = get_pci_ids(device.bus, device.device, device.function);
    if (ids.class != 0x1 || ids.subclass != 0x6 || ids.prog_if != 0x1) {
        return; // Not an AHCI controller
    }
    /* Actual driver code */
    kprintf("[AHCI] Device %u:%u.%u is an AHCI controller\n", device.bus, device.device, device.function);
    uint32_t bar5 = pci_get_bar(device, 5);
    uint32_t bar_size = pci_get_bar_size(device, 5);
    kprintf("[AHCI] BAR5: %x\n", bar5);
    kprintf("[AHCI] BAR5 size: %x\n", bar_size);
}