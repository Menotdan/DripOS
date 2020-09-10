#include "intel_hda.h"
#include "klibc/logger.h"
#include "klibc/stdlib.h"
#include "mm/vmm.h"

uint32_t controller_count = 1;
uint32_t controllers[] = {0x80862668};

hda_audio_controller_t *hda_controller_list = (void *) 0;
uint32_t hda_controller_count = 0;

void add_controller(hda_audio_controller_t controller) {
    hda_controller_list = krealloc(hda_controller_list, sizeof(hda_audio_controller_t) * (hda_controller_count + 1));
    hda_controller_list[hda_controller_count++] = controller;
}

void init_hda_controller(pci_device_t device) {
    pci_id_t ids = get_pci_ids(device.bus, device.device, device.function);

    uint8_t found = 0;
    for (uint64_t i = 0; i < controller_count; i++) {
        if (controllers[i] == (uint32_t) (ids.device_id | (ids.vendor_id << 16))) {
            found = 1;
            break;
        }
    }

    if (!found) {
        return;
    }

    hda_audio_controller_t new_controller;
    new_controller.register_address = GET_HIGHER_HALF(hda_audio_register_t *, pci_get_mmio_bar(device, 0));
    add_controller(new_controller);
    log("{-HDA-} DMA buffer: %lx", ((uint64_t) new_controller.register_address->dma_position_lower_addr + ((uint64_t) new_controller.register_address->dma_position_higher_addr << 32)));
}