#ifndef PCI_HDA_INTEL_H
#define PCI_HDA_INTEL_H
#include "drivers/pci.h"

typedef struct {
    uint16_t hda_gcap;
    uint8_t min_ver;
    uint8_t maj_ver;
    uint16_t out_payload_cap;
    uint16_t in_payload_cap;
    uint32_t global_control;
    uint16_t wake_enable;
    uint16_t wake_status;
    uint16_t global_status;
    uint8_t reserved1[6];
    uint16_t output_stream_cap;
    uint16_t input_stream_cap;
    uint8_t reserved2[4];
    uint32_t interrupt_control;
    uint32_t interrupt_status;
    uint8_t reserved3[8];
    uint32_t wall_clock;
    uint8_t reserved4[4];
    uint32_t stream_sync;
    uint8_t reserved5[4];
    uint32_t corb_lower_addr;
    uint32_t corb_higher_addr;
    uint16_t corb_write_ptr;
    uint16_t corb_read_ptr;
    uint8_t corb_control;
    uint8_t corb_status;
    uint8_t corb_size;
    uint8_t reserved6;
    uint32_t rirb_lower_addr;
    uint32_t rirb_higher_addr;
    uint16_t rirb_write_ptr;
    uint16_t response_int_count;
    uint8_t rirb_control;
    uint8_t rirb_status;
    uint8_t rirb_size;
    uint8_t reserved7;
    uint32_t command_output_interface;
    uint32_t response_input_interface;
    uint16_t command_status;
    uint8_t reserved8[6];
    uint32_t dma_position_lower_addr;
    uint32_t dma_position_higher_addr;
    uint8_t reserved9[8];
} __attribute__((packed)) hda_audio_register_t;

typedef struct {
    hda_audio_register_t *register_address;
} hda_audio_controller_t;

void init_hda_controller(pci_device_t device);

#endif