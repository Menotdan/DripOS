#include "elf.h"
#include "fs/fd.h"
#include "mm/vmm.h"
#include "mm/pmm.h"
#include "proc/scheduler.h"
#include "drivers/serial.h"
#include "klibc/string.h"
#include "klibc/stdlib.h"

process_t *load_elf(char *path) {
    sprintf("[ELF] Attempting to load %s\n", path);
    int fd = fd_open(path, 0);
    if (fd < 0) {
        sprintf("Loading elf failed! path: %s, error: %d\n", path, fd);
        return;
    }
    char elf_magic[4];
    fd_seek(fd, 0, 0);
    fd_read(fd, elf_magic, 4);

    if (strncmp("\x7f""ELF", elf_magic, 4)) {
        sprintf("Elf magic invalid!\n");
        fd_close(fd);
        return;
    }

    elf_ehdr_t *ehdr = kmalloc(sizeof(elf_ehdr_t));
    fd_seek(fd, 0, 0);
    fd_read(fd, ehdr, sizeof(elf_ehdr_t));
    if (ehdr->iden_bytes[4] != 2) {
        sprintf("32-bit ELF! (iden_bytes[4] == %u) Not loading.\n", ehdr->iden_bytes[4]);
    }
    sprintf("Entry addr = %lx\n", ehdr->e_entry);

    /* Read the phdrs and shdrs */
    elf_phdr_t *phdrs = kmalloc(sizeof(elf_phdr_t) * ehdr->e_phnum);
    elf_shdr_t *shdrs = kmalloc(sizeof(elf_shdr_t) * ehdr->e_shnum);
    fd_seek(fd, ehdr->e_shoff, 0);
    for (uint64_t i = 0; i < ehdr->e_shnum; i++) {
        fd_read(fd, (void *) ((uint64_t) shdrs + (i * sizeof(elf_shdr_t))), sizeof(elf_shdr_t));
        sprintf("Loaded an shdr.\n");
    }

    fd_seek(fd, ehdr->e_phoff, 0);
    for (uint64_t i = 0; i < ehdr->e_phnum; i++) {
        fd_read(fd, (void *) ((uint64_t) phdrs + (i * sizeof(elf_phdr_t))), sizeof(elf_phdr_t));
        sprintf("Loaded a phdr.\n");
    }
    sprintf("Loaded phdrs and shdrs!\n");

    uint64_t shstr_index = ehdr->e_shstrndx;
    sprintf("shstr_size = %lu, shstr_offset = %lu, shstr_type = %lu, shstr_index = %lu\n", shdrs[shstr_index].sh_size, shdrs[shstr_index].sh_offset, shdrs[shstr_index].sh_type, shstr_index);
    if (shdrs[shstr_index].sh_size == 0) {
        sprintf("[ELF] Error! The shstr size is 0!\n");
    }
    void *shstr_data = kmalloc(shdrs[shstr_index].sh_size);
    fd_seek(fd, shdrs[shstr_index].sh_offset, 0);
    fd_read(fd, shstr_data, shdrs[shstr_index].sh_size);

    sprintf("Sections:\n");
    for (uint64_t i = 0; i < ehdr->e_shnum; i++) {
        sprintf("  %s: Address = %lx, Size = %lx\n", (uint64_t) shstr_data + shdrs[i].sh_name, shdrs[i].sh_addr, shdrs[i].sh_size);
    }

    sprintf("Elf loaded correctly!\n");

    void *elf_address_space = vmm_fork_higher_half((void *) base_kernel_cr3);
    process_t *elf_process = create_process(path, elf_address_space);

    fd_close(fd);
    return elf_process;
}