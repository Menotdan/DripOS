#include "rsdt.h"
#include "mm/vmm.h"
#include "drivers/tty/tty.h"

rsdp1_t *rsdp;

uint64_t checksum_calc(uint8_t *data, uint64_t length) {
    uint64_t total = 0;
    for (uint64_t i = 0; i < length; i++) {
        total += (uint64_t) *data;
        data++;
    }

    return total & 0xff;
}

rsdp1_t *get_rsdp1(stivale_info_t *bootloader_info) {
    rsdp1_t *ret = GET_HIGHER_HALF(rsdp1_t *, bootloader_info->rsdp);
    rsdp1_t *phys = GET_LOWER_HALF(rsdp1_t *, ret);
    vmm_map(phys, ret, 2, VMM_PRESENT | VMM_WRITE);
    return ret;
}

sdt_header_t *get_root_sdt_header() {
    //kprintf("\n[ACPI] Finding rsdt");
    if (rsdp) {
        //kprintf("\n[ACPI] Found rsdp");
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
                    //kprintf("\n[ACPI] RSDT ACPI 2: %lx", header);
                    return header;
                } else {
                    //kprintf("\n[ACPI] Bad RSDT ACPI 2 checksum!");
                    return (sdt_header_t *) 0;
                }
            } else {
                //kprintf("\n[ACPI] Bad RSDT 1 checksum on ACPI 2");
                return (sdt_header_t *) 0;
            }
        } else {
            header = (sdt_header_t *) ((uint64_t) rsdp->rsdt_address + 0xFFFF800000000000);
            vmm_map((void *) ((uint64_t) rsdp->rsdt_address), (void *) header, 1, VMM_PRESENT | VMM_WRITE);
            vmm_map((void *) ((uint64_t) rsdp->rsdt_address), (void *) header, (header->length + 0x1000 - 1) / 0x1000,
                VMM_PRESENT | VMM_WRITE);
            uint64_t header_check = checksum_calc((uint8_t *) header, header->length);
            if (!header_check) {
                //kprintf("\n[ACPI] RSDT acpi 1: %lx", header);
                return header;
            } else {
                //kprintf("\n[ACPI] Bad RSDT acpi 1");
                return (sdt_header_t *) 0;
            }
        }
    } else {
        //kprintf("\n[ACPI] No rsdp found");
        return (sdt_header_t *) 0;
    }
}

/* Search for header with signature sig */
sdt_header_t *search_sdt_header(char *sig) {
    //kprintf("\n[ACPI] Finding SDT %s", sig);
    sdt_header_t *root = get_root_sdt_header();

    if (root) {
        //kprintf("\nFound root sdt");
        if (rsdp) {
            if (rsdp->revision == 0) {
                rsdt_t *rsdt = (rsdt_t *) root;
                //kprintf("\n[ACPI] Found RSDT with header length %u", rsdt->header.length);
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
                            //kprintf("\n[ACPI] Found correct string sig");
                            uint64_t check = checksum_calc((uint8_t *) header, header->length);

                            if (!check) {
                                //kprintf("\n[ACPI] Found checksum");
                                return header;
                            }
                        }
                    } else {
                        continue;
                    }
                }
            } else {
                xsdt_t *xsdt = (xsdt_t *) root;
                //kprintf("\n[ACPI] Found XSDT with header length %u", xsdt->header.length);
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
                            //kprintf("\n[ACPI] Found correct string sig");
                            uint64_t check = checksum_calc((uint8_t *) header, header->length);

                            if (!check) {
                                //kprintf("\n[ACPI] Found checksum");
                                return header;
                            }
                        }
                    } else {
                        continue;
                    }
                }
            }
        } else {
            //kprintf("\n[ACPI] Bad RSDP");
            return (sdt_header_t *) 0;
        }
    } else {
        //kprintf("\n[ACPI] No root SDT");
        return (sdt_header_t *) 0;
    }

    //kprintf("\n[ACPI] Didn't find %s", sig);
    return (sdt_header_t *) 0;
}

void acpi_init(stivale_info_t *bootloader_info) {
    bootloader_info = GET_HIGHER_HALF(stivale_info_t *, bootloader_info);
    rsdp = get_rsdp1(bootloader_info);
}