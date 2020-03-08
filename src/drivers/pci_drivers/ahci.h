#ifndef PCI_AHCI_H
#define PCI_AHCI_H
#include <stdint.h>
#include "drivers/pci.h"

#define	SATA_SIG_ATA	0x00000101	// SATA drive
#define	SATA_SIG_ATAPI	0xEB140101	// SATAPI drive
#define	SATA_SIG_SEMB	0xC33C0101	// Enclosure management bridge
#define	SATA_SIG_PM	0x96690101	// Port multiplier

typedef enum
{
	FIS_TYPE_REG_H2D	= 0x27,	// Register FIS - host to device
	FIS_TYPE_REG_D2H	= 0x34,	// Register FIS - device to host
	FIS_TYPE_DMA_ACT	= 0x39,	// DMA activate FIS - device to host
	FIS_TYPE_DMA_SETUP	= 0x41,	// DMA setup FIS - bidirectional
	FIS_TYPE_DATA		= 0x46,	// Data FIS - bidirectional
	FIS_TYPE_BIST		= 0x58,	// BIST activate FIS - bidirectional
	FIS_TYPE_PIO_SETUP	= 0x5F,	// PIO setup FIS - device to host
	FIS_TYPE_DEV_BITS	= 0xA1,	// Set device bits FIS - device to host
} FIS_TYPE;

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
    uint32_t command_issue;
    uint32_t sata_notif;
    uint32_t fis_base_switch;

    uint32_t reserved1[11];
    uint32_t vendor[4];
} ahci_port_t;

typedef volatile struct {
    uint32_t cap;
    uint32_t global_host_control;
    uint32_t isr_status;
    uint32_t port_implemented;
    uint32_t version;
    uint32_t ccc_control;
    uint32_t ccc_ports;
    uint32_t enclosure_location;
    uint32_t enclosure_control;
    uint32_t cap2;
    uint32_t handoff_control;

    uint8_t  reserved[0x74];
    uint8_t  vendor[0x60];

    ahci_port_t ports[32];
} abar_data_t;

typedef struct {
    pci_device_t device;
    abar_data_t *ahci_bar; // Where the AHCI memory is
} ahci_controller_t;

void ahci_init_controller(pci_device_t device);

#endif