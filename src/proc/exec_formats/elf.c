#include "elf.h"
#include "fs/fd.h"
#include "mm/vmm.h"
#include "mm/pmm.h"
#include "drivers/serial.h"
#include "drivers/tty/tty.h"
#include "drivers/pit.h"
#include "klibc/string.h"
#include "klibc/stdlib.h"
#include "klibc/auxv.h"
#include <stddef.h>

void *load_elf_addrspace(char *path, uint64_t *entry_out, uint64_t base, void *export_cr3, auxv_auxc_group_t *auxv_out) {
    uint64_t time_s = global_ticks;
    uint64_t time_read = 0;

    sprintf("Reading ELF: %s\n", path);
    int fd = fd_open(path, 0);

    if (fd < 0) {
        sprintf("Loading elf failed! path: %s, error: %d\n", path, fd);
        return (void *) 0;
    }
    char elf_magic[4];

    time_s = global_ticks;
    fd_seek(fd, 0, SEEK_SET);
    fd_read(fd, elf_magic, 4);
    time_read += global_ticks - time_s;

    if (strncmp("\x7f""ELF", elf_magic, 4)) {
        sprintf("Elf magic invalid!\n");
        fd_close(fd);
        return (void *) 0;
    }

    elf_ehdr_t *ehdr = kmalloc(sizeof(elf_ehdr_t));
    time_s = global_ticks;
    fd_seek(fd, 0, SEEK_SET);
    fd_read(fd, ehdr, sizeof(elf_ehdr_t));
    time_read += global_ticks - time_s;
    if (ehdr->iden_bytes[4] != 2) {
        sprintf("32-bit ELF! (iden_bytes[4] == %u) Not loading.\n", ehdr->iden_bytes[4]);
    }

    /* Read the phdrs and shdrs */
    elf_phdr_t *phdrs = kmalloc(sizeof(elf_phdr_t) * ehdr->e_phnum);
    elf_shdr_t *shdrs = kmalloc(sizeof(elf_shdr_t) * ehdr->e_shnum);

    time_s = global_ticks;
    fd_seek(fd, ehdr->e_shoff, SEEK_SET);
    for (uint64_t i = 0; i < ehdr->e_shnum; i++) {
        fd_read(fd, (void *) ((uint64_t) shdrs + (i * sizeof(elf_shdr_t))), sizeof(elf_shdr_t));
    }

    fd_seek(fd, ehdr->e_phoff, SEEK_SET);
    for (uint64_t i = 0; i < ehdr->e_phnum; i++) {
        fd_read(fd, (void *) ((uint64_t) phdrs + (i * sizeof(elf_phdr_t))), sizeof(elf_phdr_t));
    }
    time_read += global_ticks - time_s;
    sprintf("Read 1: %lu\n", time_read);

    uint64_t shstr_index = ehdr->e_shstrndx;
    void *shstr_data = kmalloc(shdrs[shstr_index].sh_size);

    time_s = global_ticks;
    fd_seek(fd, shdrs[shstr_index].sh_offset, SEEK_SET);
    fd_read(fd, shstr_data, shdrs[shstr_index].sh_size);
    time_read += global_ticks - time_s;

    int loaded_dynamic_linker = 0;
    uint64_t dynamic_linker_entry = 0;
    auxv_t *auxv = NULL;
    int auxc = 0;

    void *elf_address_space;
    if (!export_cr3) {
        elf_address_space = vmm_fork_higher_half((void *) base_kernel_cr3);
    } else {
        elf_address_space = export_cr3;
    }

    for (uint64_t i = 0; i < ehdr->e_phnum; i++) {
        if (phdrs[i].p_type == PT_LOAD) {
            if (phdrs[i].p_memsz == 0) continue; // Empty phdr :/
            uint64_t pages = (phdrs[i].p_memsz + 0x1000 - 1) / 0x1000;

            void *virt = (void *) phdrs[i].p_vaddr + base;
            void *region_phys = pmm_alloc(phdrs[i].p_memsz);
            void *region_virt = GET_HIGHER_HALF(void *, region_phys);
            region_virt = (void *) ((uint64_t) region_virt + ((phdrs[i].p_vaddr + base) & 0xfff));
            memset(region_virt, 0, phdrs[i].p_memsz);

            time_s = global_ticks;
            fd_seek(fd, phdrs[i].p_offset, SEEK_SET);
            fd_read(fd, region_virt, phdrs[i].p_filesz);
            time_read += global_ticks - time_s;

            vmm_remap_pages(region_phys, virt, elf_address_space, pages, VMM_PRESENT | VMM_USER | VMM_WRITE);
        } else if (phdrs[i].p_type == PT_INTERP) {
            char *ld_path = kcalloc(phdrs[i].p_filesz + 1);

            time_s = global_ticks;
            fd_seek(fd, phdrs[i].p_offset, SEEK_SET);
            fd_read(fd, ld_path, phdrs[i].p_filesz);
            time_read += global_ticks - time_s;
            time_s = global_ticks;
            load_elf_addrspace(ld_path, &dynamic_linker_entry, 0x800000000, elf_address_space, NULL);
            sprintf("Dyn Linker Load: %lu\n", global_ticks - time_s);

            loaded_dynamic_linker = 1;

            auxv_t new_auxv;
            new_auxv.a_type = AT_ENTRY;
            new_auxv.a_un.a_ptr = (void *) (ehdr->e_entry + base);
            auxv = krealloc(auxv, (auxc + 1) * sizeof(auxv_t));
            auxv[auxc++] = new_auxv;

            new_auxv.a_type = AT_PHNUM;
            new_auxv.a_un.a_val = (long) ehdr->e_phnum;
            auxv = krealloc(auxv, (auxc + 1) * sizeof(auxv_t));
            auxv[auxc++] = new_auxv;

            new_auxv.a_type = AT_PHENT;
            new_auxv.a_un.a_val = sizeof(elf_phdr_t);
            auxv = krealloc(auxv, (auxc + 1) * sizeof(auxv_t));
            auxv[auxc++] = new_auxv;
        } else if (phdrs[i].p_type == PT_PHDR) {
            auxv_t new_auxv;
            new_auxv.a_type = AT_PHDR;
            new_auxv.a_un.a_ptr = (void *) phdrs[i].p_vaddr + base;
            auxv = krealloc(auxv, (auxc + 1) * sizeof(auxv_t));
            auxv[auxc++] = new_auxv;
        } else {
            continue;
        }
    }

    if (loaded_dynamic_linker) {
        *entry_out = dynamic_linker_entry;
    } else {
        *entry_out = ehdr->e_entry + base;
        void *virt_stack = (void *) USER_STACK_START;
        void *phys_stack_region = pmm_alloc(USER_STACK_SIZE);
        void *virt_stack_region = GET_HIGHER_HALF(void *, phys_stack_region);
        memset(virt_stack_region, 0, USER_STACK_SIZE);

        vmm_map_pages(phys_stack_region, virt_stack, elf_address_space, USER_STACK_PAGES, 
            VMM_PRESENT | VMM_USER | VMM_WRITE);
    }

    if (auxv_out) {
        auxv_out->auxv = auxv;
        auxv_out->auxc = auxc;
    } else {
        if (auxv) {
            kfree(auxv);
        }
    }

    fd_close(fd);

    sprintf("Time Read: %lu\n", time_read);
    return elf_address_space;
}

int64_t load_elf(char *path) {
    uint64_t entry_point = 0;
    auxv_auxc_group_t auxv_info;
    void *address_space = load_elf_addrspace(path, &entry_point, 0, NULL, &auxv_info);
    if (!address_space) {
        return -1;
    }

    process_t *process = create_process(path, address_space);
    thread_t *thread = create_thread(path, (void *) entry_point, USER_STACK, 3);
    int64_t pid = add_process(process);

    if (auxv_info.auxv) {
        thread->vars.auxc = auxv_info.auxc;
        thread->vars.auxv = auxv_info.auxv;
    }
    add_argv(&thread->vars, path);
    add_new_child_thread(thread, pid);

    return pid;
}