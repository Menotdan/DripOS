#ifndef PCI_AHCI_H
#define PCI_AHCI_H
#include <stdint.h>
#include "drivers/pci.h"

#define	SATA_SIG_ATA	0x00000101	// SATA drive
#define	SATA_SIG_ATAPI	0xEB140101	// SATAPI drive
#define	SATA_SIG_SEMB	0xC33C0101	// Enclosure management bridge
#define	SATA_SIG_PM	0x96690101	// Port multiplier

typedef enum {
	FIS_TYPE_REG_H2D	= 0x27,	// Register FIS - host to device
	FIS_TYPE_REG_D2H	= 0x34,	// Register FIS - device to host
	FIS_TYPE_DMA_ACT	= 0x39,	// DMA activate FIS - device to host
	FIS_TYPE_DMA_SETUP	= 0x41,	// DMA setup FIS - bidirectional
	FIS_TYPE_DATA		= 0x46,	// Data FIS - bidirectional
	FIS_TYPE_BIST		= 0x58,	// BIST activate FIS - bidirectional
	FIS_TYPE_PIO_SETUP	= 0x5F,	// PIO setup FIS - device to host
	FIS_TYPE_DEV_BITS	= 0xA1,	// Set device bits FIS - device to host
} FIS_TYPE;

typedef enum {
    ATA_COMMAND_IDENTIFY = 0xEC
} ATA_COMMAND;

/* Different FISes */
typedef struct {
    uint8_t type; // FIS_TYPE_REG_H2D
    struct {
        uint8_t pm_port : 4;
        uint8_t reserved : 3;
        uint8_t c : 1;
    } flags;

    uint8_t command;
    uint8_t features;
    uint8_t lba_0;
    uint8_t lba_1;
    uint8_t lba_2;
    uint8_t dev_head;
    uint8_t lba_3;
    uint8_t lba_4;
    uint8_t lba_5;
    uint8_t features_exp;
    uint8_t sector_count_low;
    uint8_t sector_count_high;
    uint8_t reserved;
    uint8_t control;
    uint8_t reserved_0[4];
} __attribute__((packed)) ahci_h2d_fis_t;


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
} __attribute__((packed)) abar_data_t;

typedef struct {
    pci_device_t device;
    abar_data_t *ahci_bar; // Where the AHCI memory is
} ahci_controller_t;

typedef struct {
    ahci_port_t *port;
    uint8_t addresses_64;
    uint8_t command_slot_count;
} ahci_port_data_t;

typedef struct {
    struct {
        uint32_t command_fis_len : 5;
        uint32_t atapi : 1;
        uint32_t write : 1;
        uint32_t prefetchable : 1;
        uint32_t sata_reset_control : 1;
        uint32_t bist : 1;
        uint32_t clear : 1;
        uint32_t reserved : 1;
        uint32_t pmp : 4;
        uint32_t prdt_count : 16;
    } __attribute__((packed)) flags;

    uint32_t prdbc;

    uint32_t command_entry_ptr;
    uint32_t command_entry_ptr_upper;

    uint8_t reserved[16];
} __attribute__((packed)) ahci_command_header_t;

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

typedef struct {
    int index;
    ahci_command_entry_t *data;
} ahci_command_slot_t;

#define AHCI_GET_FIS_SIZE(n_fis) (sizeof(ahci_command_entry_t) + (sizeof(ahci_prdt_entry_t) * n_fis))
#define AHCI_GET_PRDT_BYTES(count) ((((count + 1) & ~1) - 1) & 0x3FFFFF)

void ahci_init_controller(pci_device_t device);

#endif