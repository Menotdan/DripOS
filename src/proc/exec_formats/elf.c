#include "elf.h"
#include "fs/fd.h"
#include "drivers/serial.h"
#include "klibc/string.h"
#include "klibc/stdlib.h"

void load_elf(char *path) {
    sprintf("[ELF] Attempting to load %s\n", path);
    int fd = fd_open(path, 0);
    if (fd < 0) {
        sprintf("[ELF] loading elf failed! path: %s, error: %d\n", path, fd);
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
    sprintf("Entry addr = %lx\n", ehdr->e_entry);

    sprintf("Elf loaded correctly!\n");
    fd_close(fd);
}