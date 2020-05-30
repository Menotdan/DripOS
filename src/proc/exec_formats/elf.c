#include "elf.h"
#include "fs/fd.h"
#include "drivers/serial.h"
#include "klibc/string.h"
#include "klibc/stdlib.h"

void load_elf(char *path) {
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
    elf_phdr_t *shdrs = kmalloc(sizeof(elf_shdr_t) * ehdr->e_shnum);
    fd_seek(fd, ehdr->e_shoff, 0);
    for (uint64_t i = 0; i < ehdr->e_shnum; i++) {
        fd_read(fd, (void *) ((uint64_t) shdrs + (i * sizeof(elf_shdr_t))), sizeof(elf_shdr_t));
    }

    fd_seek(fd, ehdr->e_phoff, 0);
    for (uint64_t i = 0; i < ehdr->e_phnum; i++) {
        fd_read(fd, (void *) ((uint64_t) phdrs + (i * sizeof(elf_phdr_t))), sizeof(elf_phdr_t));
    }
    sprintf("Loaded phdrs and shdrs!\n");

    sprintf("Elf loaded correctly!\n");
    fd_close(fd);
}