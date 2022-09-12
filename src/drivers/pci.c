#include "pci.h"
#include "io/ports.h"
#include "klibc/stdlib.h"

#include "drivers/tty/tty.h"
#include "drivers/serial.h"

/* Drivers */
#include "drivers/pci_drivers/ahci.h"
#include "drivers/pci_drivers/intel_hda.h"

const uint64_t driver_count = 1;//2;
pci_driver_t drivers[] = {ahci_init_controller};//, init_hda_controller};

pci_device_t *pci_devices;
uint64_t curent_position = 0;
uint64_t max_position = 0;

uint8_t is_mmio = 0;

/* Read PCI dword using PIO */
uint32_t pci_pio_read_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t reg) {
    /* Casts of params to 32 bit */
    uint32_t bus32 = (uint32_t) bus;
    uint32_t device32 = (uint32_t) device;
    uint32_t function32 = (uint32_t) function;

    uint32_t pci_config_address = (1 << 31) | // Enabled
        (bus32 << 16) | ((device32 & 0b11111) << 11) | ((function32 & 0b111) << 8)
        | (reg & ~(0b11)); // The register offset, with the bottom 2 bits cleared
    
    /* Actually read the data */
    port_outd(0xCF8, pci_config_address); // Write the address
    uint32_t pci_config_data = port_ind(0xCFC); // Grab the data off the port
    
    /* Return the data */
    return pci_config_data;
}

/* Write PCI dword using PIO */
void pci_pio_write_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t reg, uint32_t data) {
    /* Casts of params to 32 bit */
    uint32_t bus32 = (uint32_t) bus;
    uint32_t device32 = (uint32_t) device;
    uint32_t function32 = (uint32_t) function;

    uint32_t pci_config_address = (1 << 31) | // Enabled
        (bus32 << 16) | ((device32 & 0b11111) << 11) | ((function32 & 0b111) << 8)
        | (reg & ~(0b11)); // The register offset, with the bottom 2 bits cleared
    
    /* Actually write the data */
    port_outd(0xCF8, pci_config_address); // Write the address
    port_outd(0xCFC, data); // Put the data on the port
}

/* Read a PCI dword */
uint32_t pci_read_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t reg) {
    if (!is_mmio) {
        return pci_pio_read_dword(bus, device, function, reg);
    } else {
        return pci_pio_read_dword(bus, device, function, reg); // TODO: Add MMIO PCI
    }
}

/* Write a PCI dword */
void pci_write_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t reg, uint32_t data) {
    if (!is_mmio) {
        pci_pio_write_dword(bus, device, function, reg, data);
    } else {
        pci_pio_write_dword(bus, device, function, reg, data); // TODO: Add MMIO PCI
    }
}

/* Is a device multifunction? */
uint8_t is_multifunction(uint8_t bus, uint8_t device) {
    uint8_t header_type = (uint8_t) (pci_read_dword(bus, device, 0, 0xC) >> 16);

    return header_type & (1<<7);
}

/* Get the vendor of a bus:device.function */
uint16_t pci_get_vendor(uint8_t bus, uint8_t device, uint8_t function) {
    return (uint16_t) pci_read_dword(bus, device, function, 0);
}

/* Get the device ID */
uint16_t pci_get_device_id(uint8_t bus, uint8_t device, uint8_t function) {
    return (uint16_t) (pci_read_dword(bus, device, function, 0) >> 16);
}

/* Check if a function is present */
uint8_t check_function(uint8_t bus, uint8_t device, uint8_t function) { // Does function exist?
    return (pci_get_vendor(bus, device, function) != 0xFFFF);
}

/* Get the header type */
uint8_t get_header_type(uint8_t bus, uint8_t device, uint8_t function) {
    uint8_t header_type = (uint8_t) (pci_read_dword(bus, device, function, 0xC) >> 16);
    return header_type & ~(1<<7); // Clear the top bit before returning
}

/* Get class */
uint8_t get_class(uint8_t bus, uint8_t device, uint8_t function) {
    uint8_t class = (uint8_t) (pci_read_dword(bus, device, function, 0x8) >> 24);
    return class;
}

/* Get subclass */
uint8_t get_subclass(uint8_t bus, uint8_t device, uint8_t function) {
    uint8_t subclass = (uint8_t) (pci_read_dword(bus, device, function, 0x8) >> 16);
    return subclass;
}

/* Get prog if */
uint8_t get_prog_if(uint8_t bus, uint8_t device, uint8_t function) {
    return (uint8_t) (pci_read_dword(bus, device, function, 0x8) >> 8);
}

/* Get pci ID data */
pci_id_t get_pci_ids(uint8_t bus, uint8_t device, uint8_t function) {
    pci_id_t ret;
    ret.class = get_class(bus, device, function);
    ret.subclass = get_subclass(bus, device, function);
    ret.prog_if = get_prog_if(bus, device, function);
    ret.device_id = pci_get_device_id(bus, device, function);
    ret.vendor_id = pci_get_vendor(bus, device, function);

    return ret;
}

/* Get the secondary bus from a PCI to PCI bridge */
uint8_t get_secondary_bus(uint8_t bus, uint8_t device, uint8_t function) {
    return (uint8_t) (pci_read_dword(bus, device, function, 0x18) >> 8);
}

/* Get a BAR from a device */
uint32_t pci_get_mmio_bar(pci_device_t device, uint8_t bar) {
    return pci_read_dword(device.bus, device.device, device.function, 0x10 + (bar * 4));
}

/* Get the bar size of a PCI bar */
uint32_t pci_get_mmio_bar_size(pci_device_t device, uint8_t bar) {
    uint32_t bar_saved = pci_get_mmio_bar(device, bar);
    pci_write_dword(device.bus, device.device, device.function, 0x10 + (bar * 4), 0xFFFFFFFF);
    uint32_t ret = pci_read_dword(device.bus, device.device, device.function, 0x10 + (bar * 4));
    ret &= ~(0xF);
    ret = ~(ret) + 1;
    pci_write_dword(device.bus, device.device, device.function, 0x10 + (bar * 4), bar_saved);

    return ret;
}

uint64_t pci_device_count() {
    return curent_position;
}

pci_device_t get_pci_device(uint64_t index) {
    pci_device_t ret;
    if (index >= curent_position) {
        ret.valid = 0;
    } else {
        ret = pci_devices[index];
        ret.valid = 1;
    }
    return ret;
}

char *device_code_to_string(uint8_t class_code, uint8_t subclass, uint8_t prog_if){
    switch (class_code)
    {
    case 0: return "Undefined";
    case 1:
        switch (subclass)
        {
        case 0: return "SCSI Bus Controller";
        case 1: return "IDE Controller";
        case 2: return "Floppy Disk Controller";
        case 3: return "IPI Bus Controller";
        case 4: return "RAID Controller";
        case 5: return "ATA Controller";
        case 6:
            switch (prog_if)
            {
            case 0: return "Vendor specific SATA Controller";
            case 1: return "AHCI SATA Controller";
            case 2: return "Serial Storage Bus SATA Controller";
            }
            break;
        case 7: return "Serial Attached SCSI Controller";
        case 8:
            switch (prog_if)
            {
            case 1: return "NVMHCI Controller";
            case 2: return "NVMe Controller";
            }
            break;
        }
        return "Mass Storage Controller";
    case 2:
        switch (subclass)
        {
        case 0: return "Ethernet Controller";
        case 1: return "Token Ring Controller";
        case 2: return "FDDI Controller";
        case 3: return "ATM Controller";
        case 4: return "ISDN Controller";
        case 5: return "WorldFip Controller";
        case 6: return "PICMG 2.14 Controller";
        case 7: return "InfiniBand Controller";
        case 8: return "Fabric Controller";
        }
        return "Network Controller";
    case 3:
        switch (subclass)
        {
        case 0: return "VGA Compatible Controller";
        case 1: return "XGA Controller";
        case 2: return "3D Controller";
        }
        return "Display Controller";
    case 4: return "Multimedia controller";
    case 5: return "Memory Controller";
    case 6:
        switch (subclass)
        {
        case 0: return "Host Bridge";
        case 1: return "ISA Bridge";
        case 2: return "EISA Bridge";
        case 3: return "MCA Bridge";
        case 4: return "PCI-to-PCI Bridge";
        case 5: return "PCMCIA Bridge";
        case 6: return "NuBus Bridge";
        case 7: return "CardBus Bridge";
        case 8: return "RACEway Bridge";
        case 9: return "Semi-Transparent PCI-to-PCI Bridge";
        case 10: return "InfiniBand-to-PCI Host Bridge";
        }
        return "Bridge Device";
    case 8:
        switch (subclass)
        {
        case 0:
            switch (prog_if)
            {
            case 0: return "8259-Compatible PIC";
            case 1: return "ISA-Compatible PIC";
            case 2: return "EISA-Compatible PIC";
            case 0x10: return "I/O APIC IRQ Controller";
            case 0x20: return "I/O xAPIC IRQ Controller";
            }
            break;
        case 1:
            switch (prog_if)
            {
            case 0: return "8237-Compatible DMA Controller";
            case 1: return "ISA-Compatible DMA Controller";
            case 2: return "EISA-Compatible DMA Controller";
            }
            break;
        case 2:
            switch (prog_if)
            {
            case 0: return "8254-Compatible PIT";
            case 1: return "ISA-Compatible PIT";
            case 2: return "EISA-Compatible PIT";
            case 3: return "HPET";
            }
            break;
        case 3: return "Real Time Clock";
        case 4: return "PCI Hot-Plug Controller";
        case 5: return "SDHCI";
        case 6: return "IOMMU";
        }
        return "Base System Peripheral";
    case 0xC:
        switch (subclass)
        {
        case 0:
            switch (prog_if)
            {
            case 0: return "Generic FireWire (IEEE 1394) Controller";
            case 0x10: return "OHCI FireWire (IEEE 1394) Controller";
            }
            break;
        case 1: return "ACCESS Bus Controller";
        case 2: return "SSA Controller";
        case 3:
            switch (prog_if)
            {
            case 0: return "uHCI USB1 Controller";
            case 0x10: return "oHCI USB1 Controller";
            case 0x20: return "eHCI USB2 Controller";
            case 0x30: return "xHCI USB3 Controller";
            case 0xFE: return "USB Device";
            }
            break;
        case 4: return "Fibre Channel Controller";
        case 5: return "SMBus";
        case 6: return "InfiniBand Controller";
        case 7: return "IPMI Interface Controller";
        }
        return "Serial Bus Controller";
    default:
        return "Unknown";
        break;
    }
}

/* Is this a PCI bridge? */
uint8_t is_pci_bridge(uint8_t bus, uint8_t device, uint8_t function) {
    if (get_header_type(bus, device, function) != 0x1) return 0;
    if (get_class(bus, device, function) != 0x6) return 0;
    if (get_subclass(bus, device, function) != 0x4) return 0;

    return 1;
}

/* Add a PCI device to the list */
void add_pci_device(pci_device_t new_device) {
    pci_devices[curent_position] = new_device;
    if (curent_position == max_position) {
        pci_devices = krealloc(pci_devices, sizeof(pci_device_t) * (10 + max_position + 1));
        max_position += 10;
    }
    curent_position++;
}

/* Scan a bus rescursively */
void scan_bus(uint8_t bus) {
    kprintf("Checking bus %u\n", (uint32_t) bus);
    for (uint8_t dev = 0; dev < 32; dev++) { // Loop all devices
        if (check_function(bus, dev, 0)) { // If this is even a valid device
            if (is_pci_bridge(bus, dev, 0)) { // Is this a PCI bridge
                scan_bus(get_secondary_bus(bus, dev, 0)); // Scan the new bridge
            } else {
                pci_device_t new_pci_device = {1, bus, dev, 0};
                add_pci_device(new_pci_device);
            }
    
            if (is_multifunction(bus, dev)) { // If this is a multifunction device
                for (uint8_t func = 1; func < 8; func++) { // Loop all remaining functions
                    if (check_function(bus, dev, func)) { // If this function is valid
                        if (is_pci_bridge(bus, dev, func)) { // Is this a PCI bridge
                            scan_bus(get_secondary_bus(bus, dev, func)); // Scan the new bridge
                        } else {
                            // Add this bus:device.function to the list
                            pci_device_t new_pci_device = {1, bus, dev, func};
                            add_pci_device(new_pci_device);
                        }
                    }
                }
            }
        } else {
            continue;
        }
    }
}

void pci_enable_busmastering(pci_device_t device) {
    if (!(pci_read_dword(device.bus, device.device, device.function, 0x4) & (1 << 2))) {
        pci_write_dword(device.bus, device.device, device.function, 0x4, pci_read_dword(device.bus, device.device, device.function, 0x4) | (1 << 2));
    }
}

/* Set up the list, scan bus 0, and print all PCI devices */
void pci_init() {
    pci_devices = kcalloc(sizeof(pci_device_t) * 10); // 10 pci devices by default
    max_position = 9;
    /* Do device scans */
    scan_bus(0); // Check all bus 0 devices, and then if we find bridges, use those as well
    for (uint64_t device = 0; device < curent_position; device++) {
        pci_device_t dev = pci_devices[device];
        kprintf("[PCI] Vendor: %x on %u:%u.%u\n", (uint32_t) pci_get_vendor(dev.bus, dev.device, dev.function), (uint32_t) dev.bus, (uint32_t) dev.device, (uint32_t) dev.function);
        pci_id_t ids = get_pci_ids(dev.bus, dev.device, dev.function);
        kprintf("[PCI] Device type: %s\n", device_code_to_string(ids.class, ids.subclass, ids.prog_if));
        kprintf("[PCI] Device ID: %x\n", (uint32_t) ids.device_id);
        for (uint64_t d = 0; d < driver_count; d++) {
            drivers[d](dev); // The driver can handle checking if the device is correct
        }
    }
}