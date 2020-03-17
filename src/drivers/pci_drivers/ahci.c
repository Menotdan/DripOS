#include "ahci.h"
#include "mm/vmm.h"
#include "mm/pmm.h"
#include "fs/devfs/devfs.h"
#include "klibc/dynarray.h"
#include "klibc/string.h"
#include "klibc/stdlib.h"
#include "drivers/pit.h"
#include "proc/scheduler.h"
#include "klibc/errno.h"

#include "drivers/serial.h"
#include "drivers/tty/tty.h"

dynarray_t ahci_controllers = {0, 0, 0};

uint8_t sata_device_count = 0;
lock_t ahci_lock = 0;

int ahci_read(fd_entry_t *fd_data, void *buf, uint64_t count) {
    vfs_node_t *node = fd_data->node;

    ahci_port_data_t *port_data_for_device = get_device_data(node);
    if (port_data_for_device) {
        sprintf("\nReading from sata bytes to %lx", buf);
        int err = ahci_read_sata_bytes(port_data_for_device, buf, count, fd_data->seek);
        sprintf("\nErr: %d", err);

        if (err) {
            get_thread_locals()->errno = -EIO;
        } else {
            get_thread_locals()->errno = 0;
            fd_data->seek += count;
        }
        return 0;
    } else {
        get_thread_locals()->errno = -EIO;
        return 0;
    }

    return 0;
}

int ahci_write(fd_entry_t *fd_data, void *buf, uint64_t count) {
    vfs_node_t *node = fd_data->node;

    ahci_port_data_t *port_data_for_device = get_device_data(node);
    if (port_data_for_device) {
        int err = ahci_write_sata_bytes(port_data_for_device, buf, count, fd_data->seek);
        sprintf("\nErr: %d", err);

        if (err) {
            get_thread_locals()->errno = -EIO;
        } else {
            get_thread_locals()->errno = 0;
            fd_data->seek += count;
        }
        return 0;
    } else {
        get_thread_locals()->errno = -EIO;
        return 0;
    }

    return 0;
}

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

int ahci_stop_cmd(ahci_port_t *port) {
    port->command &= ~(1<<0); // Clear the ST bit
    port->command &= ~(1<<4); // Clear the FRE bit

    for (uint64_t i = 0; i < 100000; i++)
        asm volatile("pause");

    // Check that the bits are not set
    if ((port->command & (1<<15)) || (port->command & (1<<14)))
        return 1;

    return 0;
}

int ahci_start_cmd(ahci_port_t *port) {
    port->command |= (1<<0); // Set the ST bit
    port->command |= (1<<4); // Set the FRE bit

    for (uint64_t i = 0; i < 100000; i++)
        asm volatile("pause");

    if (!(port->command & (1<<15)) || !(port->command & (1<<14)))
        return 1;

    return 0;
}

int ahci_find_command_slot(ahci_port_data_t *port) {
    for (int i = 0; i < port->command_slot_count; i++) {
        if (!(port->port->command_issued & (1<<i)))
            return i; // If that command is not issued
    }
    return -1;
}

ahci_command_slot_t ahci_allocate_command_slot(ahci_port_data_t *port, uint64_t fis_size) {
    ahci_command_slot_t ret = {-1, (ahci_command_entry_t *) 0};

    int index = ahci_find_command_slot(port);
    if (index == -1) {
        return ret;
    }

    ahci_command_entry_t *command_entry = pmm_alloc(fis_size);
    if (!command_entry) {
        return ret;
    }
    memset(GET_HIGHER_HALF(uint8_t *, command_entry), 0, fis_size);

    ret.index = index;
    ret.data = command_entry;

    uint64_t header_address = (port->port->command_list_base | 
        ((uint64_t) port->port->command_list_base_upper << 32));
    ahci_command_header_t *headers = GET_HIGHER_HALF(ahci_command_header_t *, header_address);

    headers[index].command_entry_ptr = (uint32_t) ((uint64_t) command_entry);
    if (port->addresses_64) {
        headers[index].command_entry_ptr_upper = (uint32_t) ((uint64_t) command_entry >> 32); 
    } else {
        if ((uint64_t) command_entry > 0xFFFFFFFF) {
            kprintf("[AHCI] Error! Allocated data past 32 bit range for a non-64 bit addressing controller!\n");
            while (1) { asm volatile("pause"); }
        }
        headers[index].command_entry_ptr_upper = 0;
    }

    return ret;
}

ahci_command_header_t *ahci_get_cmd_header(ahci_port_data_t *port, uint8_t slot) {
    uint64_t header_address = (uint64_t) (port->port->command_list_base | 
        ((uint64_t) port->port->command_list_base_upper << 32));
    ahci_command_header_t *headers = GET_HIGHER_HALF(ahci_command_header_t *, header_address);

    return headers + slot;
}

void ahci_fill_prdt(ahci_port_data_t *port, void *phys, ahci_prdt_entry_t *prdt) {
    prdt->data_base = (uint64_t) phys & 0xFFFFFFFF;

    if (port->addresses_64) {
        prdt->data_base_upper = (uint32_t) ((uint64_t) phys >> 32);
    } else {
        if ((uint64_t) phys >= 0xFFFFFFFF) {
            kprintf("[AHCI] Allocation error with 32 bit overflow!\n");
        } else {
            prdt->data_base_upper = 0;
        }
    }
}

void ahci_issue_command(ahci_port_data_t *port, int command_slot) {
    port->port->command_issued |= (1<<command_slot);
}
 
void ahci_wait_ready(ahci_port_data_t *port) {
    while (port->port->task_file & (1<<3) && port->port->task_file & (1<<7))
        asm volatile("pause");
}

void ahci_comreset_port(ahci_port_data_t *port) {
    port->port->sata_control = (port->port->sata_control & ~(0b1111)) | 0x1; // COMRESET
    sleep_no_task(2); // Delay
    port->port->sata_control = port->port->sata_control & ~(0b1111); // Resume normal state
}

void ahci_reset_command_engine(ahci_port_data_t *port) {
    /* TODO: Retry running commands, except the one that errored */
    // uint32_t running_commands = port->port->command_issued;

    port->port->command &= ~(1<<0); // Clear the ST bit

    while (port->port->command & (1<<15)) 
        asm volatile("pause");
    
    // COMRESET if drive is busy
    if (port->port->task_file & (1<<7) || port->port->task_file & (1<<3)) {
        ahci_comreset_port(port);
    }

    port->port->interrupt_status |= (1<<30);

    port->port->command |= (1<<0);
    while (!(port->port->command & (1<<15))) 
        asm volatile("pause");
}

int ahci_wait_command(ahci_port_data_t *port, int command_slot) {
    while (port->port->command_issued & (1<<command_slot)) {
        asm volatile("pause");

        // Error handling
        if (port->port->interrupt_status & (1<<30)) {
            // Task file error
            if (port->port->task_file & (1<<0)) {
                // Error with transfer
                sprintf("\n[AHCI] Error!");
                return 1;
            }
        }
    }
    
    return 0;
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

            ahci_stop_cmd(port);
            sprintf("\n[AHCI] Stopped command engine");
            uint64_t ahci_data_base = (uint64_t) pmm_alloc((32 * 32) + 256);
            uint64_t ahci_fis_base = ahci_data_base + (32 * 32);
            memset(GET_HIGHER_HALF(uint8_t *, ahci_data_base), 0, (32 * 32) + 256);

            if (controller.ahci_bar->cap & (1<<31)) {
                // Command list base
                port->command_list_base = ahci_data_base & 0xFFFFFFFF;
                port->command_list_base_upper = (ahci_data_base >> 32) & 0xFFFFFFFF;
                // Received FIS base
                port->fis_base = ahci_fis_base & 0xFFFFFFFF;
                port->fis_base_upper = (ahci_fis_base >> 32) & 0xFFFFFFFF;
            } else {
                if ((ahci_fis_base + 256) > 0xFFFFFFFF) {
                    kprintf("[AHCI] ERROR: Allocation for 32 bit ahci device has been put past 4GiB!\n");
                    continue;
                }
                // Command list base
                port->command_list_base = ahci_data_base & 0xFFFFFFFF;
                port->command_list_base_upper = 0;
                // FIS base
                port->fis_base = ahci_fis_base & 0xFFFFFFFF;
                port->fis_base_upper = 0;
            }
            sprintf("\n[AHCI] Setup the port");

            if (ahci_start_cmd(port) != 0) {
                kprintf("[AHCI] Command engine failed!\n");
                continue;
            }

            sprintf("\n[AHCI] Port ready");

            port->interrupt_status = 0xFFFFFFFF;

            uint8_t address64 = (uint8_t) ((controller.ahci_bar->cap & (1<<31)) >> 31);
            ahci_port_data_t port_data = {port, address64, (controller.ahci_bar->cap & 0b1111) + 1, 0, 0, 0};

            if (port->signature == SATA_SIG_ATA) {
                ahci_identify_sata(&port_data, 0); // Only do an identify if its a SATA drive

                ahci_port_data_t *port_data_heap = kcalloc(sizeof(ahci_port_data_t));
                memcpy((uint8_t *) &port_data, (uint8_t *) port_data_heap, sizeof(ahci_port_data_t));
                char *device_name = "satadev ";
                device_name[strlen(device_name) - 1] = 'a' + sata_device_count;
                sata_device_count++;

                vfs_ops_t ops = dummy_ops;
                ops.read = ahci_read;
                ops.write = ahci_write;
                register_device(device_name, ops, port_data_heap);
            }
        }
    }

    devfs_sprint_devices();
}

void ahci_identify_sata(ahci_port_data_t *port, uint8_t packet_interface) {
    lock(&ahci_lock);
    ahci_command_slot_t command_slot = ahci_allocate_command_slot(port, AHCI_GET_FIS_SIZE(1));
    ahci_command_header_t *header = ahci_get_cmd_header(port, command_slot.index);
    
    if (command_slot.index == -1) {
        kprintf("[AHCI] No command slot!\n");

        unlock(&ahci_lock);
        return;
    }

    // Set command header
    header->flags.prdt_count = 1;
    header->flags.write = 0;
    header->flags.command_fis_len = 5;

    // Set fis data
    ahci_h2d_fis_t *fis_area = GET_HIGHER_HALF(ahci_h2d_fis_t *, command_slot.data->command_fis_data);
    fis_area->type = FIS_TYPE_REG_H2D;
    fis_area->flags.c = 1;
    fis_area->command = packet_interface == 0 ? ATA_COMMAND_IDENTIFY : ATAPI_COMMAND_IDENTIFY;
    // For legacy things
    fis_area->dev_head = 0xA0;
    fis_area->control = 0x08;

    // Area for identify
    void *identify_region = pmm_alloc(512);
    ahci_prdt_entry_t *higher_half_prdt = GET_HIGHER_HALF(ahci_prdt_entry_t *, &(command_slot.data->prdts[0]));
    ahci_fill_prdt(port, identify_region, higher_half_prdt);
    higher_half_prdt->byte_count = AHCI_GET_PRDT_BYTES(512);

    // Actually send command
    ahci_wait_ready(port);
    ahci_issue_command(port, command_slot.index);
    int err = ahci_wait_command(port, command_slot.index);

    if (err) {
        // Print the error
        uint8_t error = (uint8_t) (port->port->task_file >> 8);
        kprintf("[AHCI] Transfer error (CI set): %u\n", (uint32_t) error);
        ahci_reset_command_engine(port);

        unlock(&ahci_lock);
        return;
    }

    // Error handling
    if (port->port->interrupt_status & (1<<30)) {
        // Task file error
        if (port->port->task_file & (1<<0)) {
            // Error with transfer
            uint8_t error = (uint8_t) (port->port->task_file >> 8);
            kprintf("[AHCI] Transfer error: %u\n", (uint32_t) error);
            ahci_reset_command_engine(port);
            pmm_unalloc(identify_region, 512);

            unlock(&ahci_lock);
            return;
        }
    }
    uint8_t *data = GET_HIGHER_HALF(uint8_t *, identify_region);

    uint16_t data_valid = *(uint16_t *) (&data[212]);
    if (!(data_valid & (1<<15)) && (data_valid & (1<<14)) && (data_valid & (1<<12))) {
        // Data is valid, including the logical sector data
        uint32_t logical_sector_size = *(uint32_t *) (&data[234]);
        port->sector_size = logical_sector_size;
    } else {
        // Data invalid
        port->sector_size = 512;
    }
    kprintf("[AHCI] Sector size = %lu bytes\n", port->sector_size);

    port->sector_count = 0;
    // Get the sector count
    port->sector_count = *(uint64_t *) (&data[200]);
    if (!port->sector_count) {
        port->sector_count = (uint64_t) (*(uint32_t *) (&data[120]));
    }

    port->lba48 = (data[167] & (1 << 2)) && (data[173] & (1 << 2));
    kprintf("[AHCI] Drive sector count: %lu, LBA48: %u\n", port->sector_count, (uint32_t) port->lba48);

    unlock(&ahci_lock);
    pmm_unalloc(identify_region, 512);
}

int ahci_read_sata_bytes(ahci_port_data_t *port, void *buf, uint64_t count, uint64_t seek) {
    uint64_t sector_offset = seek % port->sector_size;
    uint64_t sector_start = seek / port->sector_size;
    uint64_t sector_end = ((seek + count) + port->sector_size - 1) / port->sector_size;
    uint64_t sector_count = sector_end - sector_start;

    if (sector_count > 0xffff) {
        return 5; // To many bytes
    } else if (sector_count == 0) {
        return 6; // Not enough bytes
    }

    uint8_t *data_buf = pmm_alloc(sector_count * port->sector_size);
    sprintf("\nUsing %lu sectors", sector_count);
    sprintf(" at sector %lu", sector_start);
    sprintf(" until sector %lu", sector_end);
    int err = ahci_io_sata_sectors(port, data_buf, sector_count, sector_start, 0);
    if (err) {
        pmm_unalloc(data_buf, sector_count * port->sector_size);
        return err;
    }

    data_buf += sector_offset;
    sprintf("\nCopying data from %lx to %lx for %lu bytes", GET_HIGHER_HALF(uint8_t *, data_buf), buf, count);
    memcpy(GET_HIGHER_HALF(uint8_t *, data_buf), buf, count);
    sprintf("\nData %lx", *GET_HIGHER_HALF(uint64_t *, data_buf));
    data_buf -= sector_offset;

    pmm_unalloc(data_buf, sector_count * port->sector_size);
    return 0;
}

int ahci_write_sata_bytes(ahci_port_data_t *port, void *buf, uint64_t count, uint64_t seek) {
    uint64_t sector_start_offset = seek % port->sector_size;
    uint64_t sector_start = seek / port->sector_size;
    uint64_t sector_end = ((seek + count) + port->sector_size - 1) / port->sector_size;
    uint64_t sector_count = sector_end - sector_start;

    if (sector_count > 0xffff) {
        return 5; // To many bytes
    } else if (sector_count == 0) {
        return 6; // Not enough bytes
    }

    uint8_t *data_buf_temp = pmm_alloc(sector_count * port->sector_size);
    uint8_t *data_buf_end_area = data_buf_temp + ((sector_count - 1) * port->sector_size);

    /* Errors */
    int err = ahci_io_sata_sectors(port, data_buf_temp, 1, sector_start, 0);
    if (err) { 
        pmm_unalloc(data_buf_temp, sector_count * port->sector_size);
        return err; 
    }

    err = ahci_io_sata_sectors(port, data_buf_end_area, 1, sector_end - 1, 0);
    if (err) { 
        pmm_unalloc(data_buf_temp, sector_count * port->sector_size); 
        return err; 
    }

    memcpy(buf + sector_start_offset, GET_HIGHER_HALF(uint8_t *, data_buf_temp + sector_start_offset), count);

    err = ahci_io_sata_sectors(port, data_buf_temp, sector_count, sector_start, 1);
    if (err) { 
        pmm_unalloc(data_buf_temp, sector_count * port->sector_size);
        return err; 
    }

    pmm_unalloc(data_buf_temp, sector_count * port->sector_size);
    return 0;
}

int ahci_io_sata_sectors(ahci_port_data_t *port, void *buf, uint16_t count, uint64_t offset, uint8_t write) {
    lock(&ahci_lock);

    uint64_t prdt_count = ((count * port->sector_size) + 0x400000 - 1) / 0x400000;
    ahci_command_slot_t command_slot = ahci_allocate_command_slot(port, AHCI_GET_FIS_SIZE(prdt_count + 1));
    ahci_command_header_t *header = ahci_get_cmd_header(port, command_slot.index);

    if (command_slot.index == -1) {
        kprintf("[AHCI] No command slot!\n");

        unlock(&ahci_lock);
        return 1;
    }

    // Set command header
    header->flags.prdt_count = prdt_count;
    header->flags.write = write;
    header->flags.command_fis_len = 5;

    // Set fis data
    ahci_h2d_fis_t *fis_area = GET_HIGHER_HALF(ahci_h2d_fis_t *, command_slot.data->command_fis_data);
    fis_area->type = FIS_TYPE_REG_H2D;
    fis_area->flags.c = 1;
    fis_area->command = write == 1 ? (port->lba48 == 1 ? ATA_COMMAND_DMA_EXT_WRITE : ATA_COMMAND_DMA_WRITE) 
        : (port->lba48 == 1 ? ATA_COMMAND_DMA_EXT_READ : ATA_COMMAND_DMA_READ);
    // For legacy things
    fis_area->dev_head = 0xA0 | (1 << 6); // Add LBA mode
    fis_area->control = 0x08;

    // LBA data
    fis_area->lba_0 = (offset >> 0) && 0xFF;
    fis_area->lba_1 = (offset >> 8) && 0xFF;
    fis_area->lba_2 = (offset >> 16) && 0xFF;

    if (port->lba48 == 1) {
        fis_area->lba_3 = (offset >> 24) && 0xFF;
        fis_area->lba_4 = (offset >> 32) && 0xFF;
        fis_area->lba_5 = (offset >> 40) && 0xFF;
    }

    if (count != 0xffff) {
        fis_area->sector_count_low = (count >> 0) & 0xFF;
        fis_area->sector_count_high = (count >> 8) & 0xFF;
    } else {
        fis_area->sector_count_low = 0;
        fis_area->sector_count_high = 0;
    }

    char *local_buf = buf;

    uint64_t bytes_left = count * port->sector_size;
    for (uint64_t i = 0; i < prdt_count; i++) {
        ahci_prdt_entry_t *higher_half_prdt = GET_HIGHER_HALF(ahci_prdt_entry_t *, &(command_slot.data->prdts[i]));
        ahci_fill_prdt(port, local_buf + (i * 0x400000), higher_half_prdt);

        sprintf("\n[AHCI] Bytes left: %lu", bytes_left);

        if (bytes_left >= 0x400000) {
            higher_half_prdt->byte_count = AHCI_GET_PRDT_BYTES(0x400000);
            bytes_left -= 0x400000;
        } else {
            higher_half_prdt->byte_count = AHCI_GET_PRDT_BYTES(bytes_left);
            sprintf("\n[AHCI] Byte count: %x", higher_half_prdt->byte_count);
            bytes_left = 0;
        }
    }

    // Actually send command
    ahci_wait_ready(port);
    ahci_issue_command(port, command_slot.index);
    int err = ahci_wait_command(port, command_slot.index);

    if (err) {
        // Print the error
        uint8_t error = (uint8_t) (port->port->task_file >> 8);
        kprintf("[AHCI] Transfer error (CI set): %u\n", (uint32_t) error);
        ahci_reset_command_engine(port);

        unlock(&ahci_lock);
        return 2;
    }

    // Error handling
    if (port->port->interrupt_status & (1<<30)) {
        // Task file error
        if (port->port->task_file & (1<<0)) {
            // Error with transfer
            uint8_t error = (uint8_t) (port->port->task_file >> 8);
            kprintf("[AHCI] Transfer error: %u\n", (uint32_t) error);
            ahci_reset_command_engine(port);

            unlock(&ahci_lock);
            return 3;
        }
    }

    unlock(&ahci_lock);
    return 0; // Return success
}

void ahci_init_controller(pci_device_t device) {
    pci_id_t ids = get_pci_ids(device.bus, device.device, device.function);
    if (ids.class != 0x1 || ids.subclass != 0x6 || ids.prog_if != 0x1) {
        return; // Not an AHCI controller
    }

    /* Actual driver code (lol) */
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