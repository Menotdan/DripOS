#ifndef ELF_H
#define ELF_H
#include <stdint.h>
#include "proc/scheduler.h"

#define PT_LOAD 1
#define PT_INTERP 3

typedef struct elf_ehdr {
    uint8_t iden_bytes[16];
    uint16_t e_type;
	uint16_t e_machine;
	uint32_t e_version;
	uint64_t e_entry;
	uint64_t e_phoff;
	uint64_t e_shoff;
	uint32_t e_flags;
	uint16_t e_ehsize;
	uint16_t e_phentsize;
	uint16_t e_phnum;
	uint16_t e_shentsize;
	uint16_t e_shnum;
	uint16_t e_shstrndx;
} __attribute__((packed)) elf_ehdr_t;

typedef struct elf_phdr {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} __attribute__((packed)) elf_phdr_t;

typedef struct elf_shdr {
    uint32_t sh_name;
    uint32_t sh_type;
    uint64_t sh_flags;
    uint64_t sh_addr;
    uint64_t sh_offset;
    uint64_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint64_t sh_addralign;
    uint64_t sh_entsize;
} __attribute__((packed)) elf_shdr_t;

int64_t load_elf(char *path);
void *load_elf_addrspace(char *path, uint64_t *entry_out, uint64_t base, void *export_cr3, auxv_auxc_group_t *auxv_out);

#endif