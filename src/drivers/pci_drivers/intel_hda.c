#include "intel_hda.h"
#include "klibc/logger.h"
#include "klibc/stdlib.h"
#include "proc/scheduler.h"
#include "mm/vmm.h"
#include "mm/pmm.h"

uint32_t controller_count = 1;
uint32_t controllers[] = {0x80862668};

hda_audio_controller_t *hda_controller_list = (void *) 0;
uint32_t hda_controller_count = 0;

void add_controller(hda_audio_controller_t controller) {
    hda_controller_list = krealloc(hda_controller_list, sizeof(hda_audio_controller_t) * (hda_controller_count + 1));
    hda_controller_list[hda_controller_count++] = controller;
}

void reset_rirb(uint64_t controller_id) {
    if (controller_id >= hda_controller_count) {
        warn("{-HDA-} A RIRB reset on a nonexistent controller was requested!");
        return;
    }

    hda_audio_controller_t controller = hda_controller_list[controller_id];
    controller.register_address->rirb_control = controller.register_address->rirb_control & ~(1<<1);
    controller.register_address->rirb_write_ptr = controller.register_address->rirb_write_ptr | (1<<15);
    controller.register_address->rirb_control = controller.register_address->rirb_control | (1<<1);
    controller.register_address->response_int_count = 32;
}

void write_corb(uint64_t controller_id, uint32_t data) {
    if (controller_id >= hda_controller_count) {
        warn("{-HDA-} A write to a CORB on a nonexistent controller was requested!");
        return;
    }

    hda_audio_controller_t controller = hda_controller_list[controller_id];
    volatile uint32_t *corb_address = GET_HIGHER_HALF(void *, ((uint64_t) controller.register_address->corb_lower_addr | ((uint64_t) controller.register_address->corb_higher_addr << 32)));

    log("{-HDA-} Writing verb %x to corb_address %lx", data, &(corb_address[(controller.register_address->corb_write_ptr + 1) & 255]));
    corb_address[(controller.register_address->corb_write_ptr + 1) & 255] = data;
    controller.register_address->corb_write_ptr++;

    while ((controller.register_address->corb_read_ptr & 0b11111111) != (controller.register_address->corb_write_ptr & 0b11111111)) {
        log("{-HDA-} Read ptr: %lu, Write ptr: %lu", (controller.register_address->corb_read_ptr & 0b11111111), (controller.register_address->corb_write_ptr & 0b11111111));
        asm("pause");

        if ((controller.register_address->corb_status & 0x1)) {
            log("{-HDA-} Memory error bit set in CORB");
        }

        if (!(controller.register_address->corb_control & (1<<1))) {
            log("{-HDA-} CORB not enabled!");
        }
    }

    for (uint64_t i = 0; i < 2500; i++) { // Wait 1 ms
        io_wait();
    }
}

uint64_t *read_rirb(uint64_t controller_id, uint64_t *responses) {
    if (controller_id >= hda_controller_count) {
        warn("{-HDA-} A read from a RIRB on a nonexistent controller was requested!");
        return (void *) 0;
    }

    hda_audio_controller_t controller = hda_controller_list[controller_id];
    volatile uint64_t *rirb_address = GET_HIGHER_HALF(void *, ((uint64_t) controller.register_address->rirb_lower_addr | ((uint64_t) controller.register_address->rirb_higher_addr << 32)));

    uint64_t *buffer = (void *) 0;
    uint64_t response_count = 0;
    for (uint64_t i = 1; i < ((controller.register_address->rirb_write_ptr + 1) & 0b11111111); i++) {
        buffer = krealloc(buffer, (response_count + 1) * 8);
        buffer[response_count++] = rirb_address[i];
    }

    reset_rirb(controller_id);

    *responses = response_count;
    return buffer;
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
    new_controller.register_address = GET_HIGHER_HALF(hda_audio_register_t *, (pci_get_mmio_bar(device, 0) & 0xfffffff0));
    vmm_set_pat((void *) new_controller.register_address, 1, 7);

    add_controller(new_controller);
    log("{-HDA-} Controller version: %u.%u", new_controller.register_address->maj_ver, new_controller.register_address->min_ver);
    log("{-HDA-} Bringing up controller link.");

    new_controller.register_address->global_control |= 0x1; // Set the link bit

    log("{-HDA-} Reset bit set, waiting for link to be up...");
    pci_enable_busmastering(device); // Might as well do this while we wait
    while (!(new_controller.register_address->global_control & 0x1)) {
        asm("pause");
    }
    log("{-HDA-} Controller reset, link up.");
    sleep_ms(1);

    // Now we can read codecs I guess
    uint16_t codecs = new_controller.register_address->wake_status;
    for (uint8_t i = 0; i < 15; i++) {
        if (codecs & (1<<i)) {
            log("{-HDA-} Codec at address %u is present.", i);
        }
    }

    new_controller.register_address->corb_control = (new_controller.register_address->corb_control & ~(1<<1));
    io_wait();
    if ((new_controller.register_address->corb_control & (1<<1))) {
        warn("{-HDA-} CORB DMA engine failed to stop.");
        return;
    }
    log("{-HDA-} CORB DMA engine disabled.");

    uint8_t supported_sizes = (new_controller.register_address->corb_size >> 4);
    uint8_t corb_size = 0b00;
    if (supported_sizes & 0b0100) {
        corb_size = 0b10;
        hda_controller_list[hda_controller_count - 1].corb_entries = 256;
    } else if (supported_sizes & 0b0010) {
        corb_size = 0b01;
        hda_controller_list[hda_controller_count - 1].corb_entries = 8;
    } else if (supported_sizes & 0b0001) {
        corb_size = 0b00;
        hda_controller_list[hda_controller_count - 1].corb_entries = 2;
    } else {
        warn("{-HDA-} Controller does not support any CORB sizes!");
        return;
    }

    if (new_controller.register_address->corb_size & 0b11) {
        log("{-HDA-} Controller already set CORB size: %u.", new_controller.register_address->corb_size & 0b11);
        switch (new_controller.register_address->corb_size & 0b11) {
            case 0b10:
                hda_controller_list[hda_controller_count - 1].corb_entries = 256;
                break;
            case 0b01:
                hda_controller_list[hda_controller_count - 1].corb_entries = 8;
                break;
            default:
                break;
        }
    } else {
        new_controller.register_address->corb_size |= corb_size;
    }

    void *corb_phys = pmm_alloc(0x1000);
    void *corb_virt = GET_HIGHER_HALF(void *, corb_phys);
    vmm_set_pat(corb_virt, 1, 7);

    memset(corb_virt, 0x12, 0x1000);

    if ((uint64_t) corb_phys > 0x100000000 && !(new_controller.register_address->hda_gcap & 0x1)) {
        warn("{-HDA-} Controller does not support 64 bit address, but a 64 bit address was allocated.");
        pmm_unalloc(corb_phys, 0x1000);
        return;
    }

    new_controller.register_address->corb_lower_addr = (uint32_t) ((uint64_t) corb_phys);
    if (new_controller.register_address->hda_gcap & 0x1) {
        new_controller.register_address->corb_higher_addr = (uint32_t) ((uint64_t) corb_phys >> 32);
    }

    new_controller.register_address->corb_write_ptr = 0;

    new_controller.register_address->corb_read_ptr = (new_controller.register_address->corb_read_ptr | (1<<15));
    io_wait();
    if (!(new_controller.register_address->corb_read_ptr & (1<<15))) {
        warn("{-HDA-} CORB read pointer reset failed.");
        return;
    }

    new_controller.register_address->corb_read_ptr = (new_controller.register_address->corb_read_ptr & ~(1<<15));
    io_wait();
    if ((new_controller.register_address->corb_read_ptr & (1<<15))) {
        warn("{-HDA-} CORB read pointer reset failed.");
        return;
    }

    log("{-HDA-} Setup CORB buffer.");

    new_controller.register_address->corb_control = (new_controller.register_address->corb_control | (1<<1));
    io_wait();
    if (!(new_controller.register_address->corb_control & (1<<1))) {
        warn("{-HDA-} CORB DMA engine failed to start.");
        return;
    }
    log("{-HDA-} CORB DMA engine enabled.");

    new_controller.register_address->rirb_control = (new_controller.register_address->rirb_control & ~(1<<1));
    io_wait();
    log("{-HDA-} RIRB DMA engine disabled.");

    uint8_t supported_rirb_sizes = (new_controller.register_address->rirb_size >> 4);
    uint8_t rirb_size = 0b00;
    if (supported_rirb_sizes & 0b0100) {
        rirb_size = 0b10;
        hda_controller_list[hda_controller_count - 1].rirb_entries = 256;
    } else if (supported_rirb_sizes & 0b0010) {
        rirb_size = 0b01;
        hda_controller_list[hda_controller_count - 1].rirb_entries = 16;
    } else if (supported_rirb_sizes & 0b0001) {
        rirb_size = 0b00;
        hda_controller_list[hda_controller_count - 1].rirb_entries = 2;
    } else {
        warn("{-HDA-} Controller does not support any RIRB sizes!");
        return;
    }

    if (new_controller.register_address->rirb_size & 0b11) {
        log("{-HDA-} Controller already set RIRB size: %u.", new_controller.register_address->rirb_size & 0b11);
        switch (new_controller.register_address->rirb_size & 0b11) {
            case 0b10:
                hda_controller_list[hda_controller_count - 1].rirb_entries = 256;
                break;
            case 0b01:
                hda_controller_list[hda_controller_count - 1].rirb_entries = 16;
                break;
            default:
                break;
        }
    } else {
        new_controller.register_address->rirb_size |= rirb_size;
    }

    new_controller.register_address->response_int_count = 32; // idk

    void *rirb_phys = pmm_alloc(0x1000);
    void *rirb_virt = GET_HIGHER_HALF(void *, rirb_phys);
    vmm_set_pat(rirb_virt, 1, 7);

    memset(rirb_virt, 0x14, 0x1000);

    if ((uint64_t) rirb_phys > 0x100000000 && !(new_controller.register_address->hda_gcap & 0x1)) {
        warn("{-HDA-} Controller does not support 64 bit address, but a 64 bit address was allocated.");
        pmm_unalloc(rirb_phys, 0x1000);
        return;
    }

    new_controller.register_address->rirb_lower_addr = (uint32_t) ((uint64_t) rirb_phys);
    if (new_controller.register_address->hda_gcap & 0x1) {
        new_controller.register_address->rirb_higher_addr = (uint32_t) ((uint64_t) rirb_phys >> 32);
    }

    new_controller.register_address->rirb_write_ptr = (new_controller.register_address->rirb_write_ptr | (1<<1));

    new_controller.register_address->rirb_control = (new_controller.register_address->rirb_control | (1<<1));
    io_wait();
    log("{-HDA-} RIRB DMA engine enabled.");

    write_corb(hda_controller_count - 1, (0xf00 << 8));

    uint64_t response_count = 0;
    uint64_t *responses = read_rirb(hda_controller_count - 1, &response_count);

    log("{-HDA-} Response count: %lu", response_count);
    for (uint64_t i = 0; i < response_count; i++) {
        log("{-HDA-} Response: %lx", responses[i]);
    }

    if (responses) {
        kfree(responses);
    }
}