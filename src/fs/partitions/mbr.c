#include "mbr.h"
#include "fs/fd.h"
#include "fs/devfs/devfs.h"
#include "klibc/stdlib.h"
#include "klibc/string.h"
#include "klibc/errno.h"
#include "proc/scheduler.h"
#include "drivers/tty/tty.h"

int mbr_open(char *name, int mode) {
    (void) name;
    (void) mode;
    return 0;
}

int mbr_close(int fd_no) {
    (void) fd_no;
    return 0;
}

int mbr_read(int fd_no, void *buf, uint64_t count) {
    fd_entry_t *fd_dat = fd_lookup(fd_no);
    mbr_part_offset_t *data = get_device_data(fd_dat->node);
    int fd = fd_open(data->drive, 0);

    fd_seek(fd, fd_dat->seek + data->offset, 0);
    int ret = fd_read(fd, buf, count);
    fd_close(fd);

    return ret;
}

int mbr_write(int fd_no, void *buf, uint64_t count) {
    fd_entry_t *fd_dat = fd_lookup(fd_no);
    mbr_part_offset_t *data = get_device_data(fd_dat->node);
    int fd = fd_open(data->drive, 0);

    fd_seek(fd, fd_dat->seek + data->offset, 0);
    int ret = fd_write(fd, buf, count);
    fd_close(fd);

    return ret;
}

int mbr_seek(int fd_no, uint64_t offset, int whence) {
    (void) fd_no;
    (void) offset;
    (void) whence;

    return 0;
}

void read_mbr(char *drive_name, uint64_t sector_size) {
    (void) sector_size;

    int fd = fd_open(drive_name, 0);
    mbr_bootsect_t *data = kcalloc(sizeof(mbr_bootsect_t));
    fd_seek(fd, 0, 0);
    fd_read(fd, data, sizeof(mbr_bootsect_t));

    kprintf("Partitions on drive %s:\n", drive_name);
    for (uint64_t i = 0; i < 4; i++) {
        kprintf("   Partition %u:\n", i);
        kprintf("     %s\n", data->partitions[i].attr ? "active" : "not active");
        kprintf("     Sector range: %u - %u\n", data->partitions[i].lba_part_start, data->partitions[i].lba_part_start + data->partitions[i].lba_sector_cnt);
        kprintf("     Attr: %u\n", data->partitions[i].attr);
    }

    kfree(data);
    fd_close(fd);
}