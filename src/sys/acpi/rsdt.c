#include "rsdt.h"
#include "mm/vmm.h"

uint64_t get_ebda() {
    uint16_t segment = *(uint16_t *) VIRTUAL_EBDA_PTR;

    if (segment == 0) {
        segment = 0x9FC0; // Default to the common EBDA address
    }

    uint64_t real_addr = ((uint64_t) segment) << 4; // Bitshift the segment so it is a correct address
    real_addr += 0xFFFF800000000000; // offset the address by virtual offset

    return real_addr;
}

int check_rsdp_string(rsdp1_t *descriptor) {
    char valid_sig[] = "RSD PTR ";
    for (uint8_t i = 0; i < 8; i++) {
        if (descriptor->signature[i] != valid_sig[i]) {
            return 1;
        }
    }
    return 0;
}

uint64_t checksum_calc(uint8_t *data, uint64_t length) {
    uint64_t total = 0;
    for (uint64_t i = 0; i < length; i++) {
        total += (uint64_t) *data;
        data++;
    }

    return total & 0xff;
}

rsdp1_t *get_rsdp1(uint64_t ebda) {
    uint64_t aligned_ebda = ebda & ~(0xf);
    uint16_t scanned = 0; // A count of the bytes scanned

    while (scanned < 1024) {
        rsdp1_t *check = (rsdp1_t *) aligned_ebda;
        if (!check_rsdp_string(check)) {
            // If the string is correct
            // check the checksum

            if (!checksum_calc((uint8_t *) check, sizeof(rsdp1_t))) {
                return check;
            }
        }

        scanned += 16;
        aligned_ebda += 16;
    }

    uint64_t bios_range = 0xE0000 + 0xFFFF800000000000;
    uint64_t max = 0x100000 + 0xFFFF800000000000;
    uint64_t to_scan = max - bios_range;
    
    while (to_scan > 0) {
        rsdp1_t *check = (rsdp1_t *) bios_range;
        if (!check_rsdp_string(check)) {
            // If the string is correct

            if (!checksum_calc((uint8_t *) check, sizeof(rsdp1_t))) {
                return check;
            }
        }

        bios_range += 16;
        to_scan -= 16;
    }

    return (rsdp1_t *) 0; // We found nothing :(
}

sdt_header_t *get_root_sdt_header() {
    rsdp1_t *rsdp = get_rsdp1(get_ebda());
    if (rsdp) {
        sdt_header_t *header;
        if (rsdp->revision == 2) {
            rsdp2_t *rsdp2 = (rsdp2_t *) rsdp;
            uint64_t check = checksum_calc((uint8_t *) ((uint64_t) rsdp2 + sizeof(rsdp1_t)), sizeof(rsdp2_t) - sizeof(rsdp1_t));

            if (!check) {
                header = (sdt_header_t *) (rsdp2->xsdt_address + 0xFFFF800000000000);
                vmm_map((void *) ((uint64_t) rsdp2->xsdt_address), (void *) header, 1, VMM_PRESENT | VMM_WRITE);
                vmm_map((void *) ((uint64_t) rsdp2->xsdt_address), (void *) header, (header->length + 0x1000 - 1) / 0x1000,
                    VMM_PRESENT | VMM_WRITE);
                uint64_t header_check = checksum_calc((uint8_t *) header, header->length);
                if (!header_check) {
                    return header;
                } else {
                    return (sdt_header_t *) 0;
                }
            } else {
                return (sdt_header_t *) 0;
            }
        } else {
            header = (sdt_header_t *) ((uint64_t) rsdp->rsdt_address + 0xFFFF800000000000);
            vmm_map((void *) ((uint64_t) rsdp->rsdt_address), (void *) header, 1, VMM_PRESENT | VMM_WRITE);
            vmm_map((void *) ((uint64_t) rsdp->rsdt_address), (void *) header, (header->length + 0x1000 - 1) / 0x1000,
                VMM_PRESENT | VMM_WRITE);
            uint64_t header_check = checksum_calc((uint8_t *) header, header->length);
            if (!header_check) {
                return header;
            } else {
                return (sdt_header_t *) 0;
            }
        }
    } else {
        return (sdt_header_t *) 0;
    }
}

/* Search for header with signature sig */
sdt_header_t *search_sdt_header(char *sig) {
    sdt_header_t *root = get_root_sdt_header();

    if (root) {
        rsdp1_t *rsdp = get_rsdp1(get_ebda());

        if (rsdp) {
            if (rsdp->revision == 0) {
                rsdt_t *rsdt = (rsdt_t *) root;
                for (uint64_t i = 0; i < (rsdt->header.length / 4); i++) {
                    sdt_header_t *header = (sdt_header_t *) ((uint64_t) rsdt->other_sdts[i] + 0xFFFF800000000000);
                    if (rsdt->other_sdts[i]) {
                        /* Map the length */
                        vmm_map((void *) ((uint64_t) rsdt->other_sdts[i]), (void *) header, 1, VMM_PRESENT | VMM_WRITE);
                        /* Map the rest of the data */
                        vmm_map((void *) ((uint64_t) rsdt->other_sdts[i]), (void *) header, 
                            (header->length + 0x1000 - 1) / 0x1000,
                            VMM_PRESENT | VMM_WRITE);

                        /* Check if the table's signature matches sig */
                        uint8_t sig_correct = 1;
                        
                        for (uint8_t s = 0; s < 4; s++) {
                            if (sig[s] != header->signature[s]) {
                                sig_correct = 0;
                                break;
                            }
                        }
                        if (sig_correct) {
                            uint64_t check = checksum_calc((uint8_t *) header, header->length);

                            if (!check) {
                                return header;
                            }
                        }
                    } else {
                        continue;
                    }
                }
            } else {
                xsdt_t *xsdt = (xsdt_t *) root;
                for (uint64_t i = 0; i < (xsdt->header.length / 8); i++) {
                    sdt_header_t *header = (sdt_header_t *) ((uint64_t) xsdt->other_sdts[i] + 0xFFFF800000000000);
                    if (xsdt->other_sdts[i]) {
                        /* Map the length */
                        vmm_map((void *) ((uint64_t) xsdt->other_sdts[i]), (void *) header, 1, VMM_PRESENT | VMM_WRITE);
                        /* Map the rest of the data */
                        vmm_map((void *) ((uint64_t) xsdt->other_sdts[i]), (void *) header, 
                            (header->length + 0x1000 - 1) / 0x1000,
                            VMM_PRESENT | VMM_WRITE);

                        /* Check if the table's signature matches sig */
                        uint8_t sig_correct = 1;
                        
                        for (uint8_t s = 0; s < 4; s++) {
                            if (sig[s] != header->signature[s]) {
                                sig_correct = 0;
                                break;
                            }
                        }
                        if (sig_correct) {
                            uint64_t check = checksum_calc((uint8_t *) header, header->length);

                            if (!check) {
                                return header;
                            }
                        }
                    } else {
                        continue;
                    }
                }
            }
        } else {
            return (sdt_header_t *) 0;
        }
    } else {
        return (sdt_header_t *) 0;
    }

    return (sdt_header_t *) 0;
}