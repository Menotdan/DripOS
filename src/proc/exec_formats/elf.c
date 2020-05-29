#include "elf.h"
#include "fs/fd.h"
#include "drivers/serial.h"
#include "klibc/string.h"

void load_elf(char *path) {
    sprintf("\n[ELF] Attempting to load %s\n", path);
    int fd = fd_open(path, 0);
    if (fd < 0) {
        sprintf("[ELF] loading elf failed! path: %s, error: %d", path, fd);
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

    sprintf("Elf loaded correctly!\n");
    fd_close(fd);
}