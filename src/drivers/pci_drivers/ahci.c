#include "ahci.h"
#include "mm/vmm.h"
#include "klibc/dynarray.h"

#include "drivers/serial.h"
#include "drivers/tty/tty.h"

dynarray_t ahci_controllers = {0, 0, 0};

uint8_t port_present(ahci_controller_t controller, uint8_t port) {
    if (controller.ahci_bar->port_implemented & (1<<port)) {
        return 1;
    }
    return 0;
}

int ahci_get_controller_ownership(ahci_controller_t controller) {
    uint16_t minor = controller.ahci_bar->version & 0xFFFF;
    uint8_t minor_2 = minor >> 8;
    uint16_t major = (controller.ahci_bar->version >> 16) & 0xFFFF;

    if (major >= 1 && minor_2 >= 2) {
        // We might need to get control of the controller
        if (controller.ahci_bar->cap2 & 1) {
            // Get ownership
            controller.ahci_bar->handoff_control |= (1<<1); // Set the OOS

            for(uint64_t i = 0; i < 100000; i++) // Wait a bit
                asm("pause");
            
            if (controller.ahci_bar->handoff_control & (1<<4)) {
                // If the BIOS is busy
                for(size_t i = 0; i < 800000; i++) // Wait for the BIOS
                    asm("pause");
            }

            // Check if we got control
            uint32_t handoff_control = controller.ahci_bar->handoff_control;
            if (!(handoff_control & (1<<1)) && (handoff_control & (1<<0) || handoff_control & (1<<4))) {
                // We don't have control :(
                kprintf("\n[AHCI] error: Failed to get ownership from BIOS");
                return 1;
            }
            controller.ahci_bar->handoff_control |= (1<<3); // Clear the OOC
            return 0;
        }
    }
    return 0; // success i guess
}

void ahci_enable_present_devs(ahci_controller_t controller) {
    // Find devices
    for (uint8_t p = 0; p < 32; p++) {
        if (port_present(controller, p)) {
            sprintf("\n[AHCI] Port %u implemented", (uint32_t) p);
            ahci_port_t *port = &controller.ahci_bar->ports[p];
            uint8_t com_status = port->sata_status & 0b1111;

            if (com_status != 3) {
                if (com_status == 0) {
                    continue;
                } else if (com_status == 1) {
                    kprintf("[AHCI] Warning: No COMRESET implemented!\n");
                    continue;
                } else if (com_status == 4) {
                    sprintf("\n[AHCI] Phy offline");
                    continue;
                } else {
                    sprintf("\n[AHCI] Unknown status");
                    continue;
                }
            }

            // Setup the device if it is present
            sprintf("\n[AHCI] Found present device %u", (uint32_t) p);
            uint8_t ipm = (port->sata_status >> 8) & 0b1111;
            if (ipm != 1) {
                kprintf("[AHCI] Warning: Device sleeping\n");
                continue;
            }

            switch (port->signature)
            {
                case SATA_SIG_ATA:
                    kprintf("[AHCI] Found SATA drive\n");
                    break;
                case SATA_SIG_ATAPI:
                    kprintf("[AHCI] Found SATAPI drive\n");
                    break;
                case SATA_SIG_PM:
                    kprintf("[AHCI] Found port multiplier\n");
                    break;
                case SATA_SIG_SEMB:
                    kprintf("[AHCI] Found enclosure management bridge\n");
                    break;
                default:
                    kprintf("[AHCI] Unknown signature %x\n", port->signature);
                    break;
            }
        }
    }
}

void ahci_init_controller(pci_device_t device) {
    pci_id_t ids = get_pci_ids(device.bus, device.device, device.function);
    if (ids.class != 0x1 || ids.subclass != 0x6 || ids.prog_if != 0x1) {
        return; // Not an AHCI controller
    }
    /* Actual driver code */
    kprintf("[AHCI] Device %u:%u.%u is an AHCI controller\n", device.bus, device.device, device.function);
    uint32_t bar5 = pci_get_mmio_bar(device, 5) & ~(0xFFF);
    uint32_t bar_size = pci_get_mmio_bar_size(device, 5);
    kprintf("[AHCI] BAR5: %x\n", bar5);
    kprintf("[AHCI] BAR5 size: %x\n", bar_size);

    /* Add the controller */
    ahci_controller_t controller = {device, (abar_data_t *) ((uint64_t) bar5 + NORMAL_VMA_OFFSET)};
    dynarray_add(&ahci_controllers, &controller, sizeof(ahci_controller_t));

    /* Map the AHCI BAR */
    vmm_map((void *) ((uint64_t) bar5), (void *) ((uint64_t) bar5 + NORMAL_VMA_OFFSET), 
        (bar_size + 0x1000 - 1) / 0x1000, VMM_PRESENT | VMM_WRITE);
    
    /* Setup the controller */
    controller.ahci_bar->global_host_control |= (1<<31); // Enable AHCI access

    // Get ownership of the controller from BIOS
    if (ahci_get_controller_ownership(controller) == 1) {
        return;
    }
    kprintf("[AHCI] Got ownership of controller\n");
    uint8_t iss = (controller.ahci_bar->cap >> 20) & 0b1111;
    sprintf("\n[AHCI] Controller link speed: ");

    switch (iss)
    {
        case 0b0001:
            sprintf("Gen 1 (1.5 GB/s)");
            break;
        case 0b0010:
            sprintf("Gen 2 (3 GB/s)");
            break;
        case 0b0011:
            sprintf("Gen 3 (6 GB/s)");
            break;
        default:
        case 0:
            sprintf("Reserved");
            break;
    }
    ahci_enable_present_devs(controller);
}