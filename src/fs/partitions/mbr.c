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
    int fd = fd_open(drive_name, 0);
    mbr_bootsect_t *data = kcalloc(sizeof(mbr_bootsect_t));
    fd_seek(fd, 0, 0);
    fd_read(fd, data, sizeof(mbr_bootsect_t));

    uint8_t found_active = 0;
    kprintf("\nPartitions on drive %s:", drive_name);
    for (uint64_t i = 0; i < 4; i++) {
        kprintf("\n   Partition %u:", i);
        kprintf("\n     %s", data->partitions[i].attr ? "active" : "not active");
        // if (data->partitions[i].attr) {
        //     found_active |= (1<<i);
        kprintf("\n     Sector range: %u - %u", data->partitions[i].lba_part_start, data->partitions[i].lba_part_start + data->partitions[i].lba_sector_cnt);
        kprintf("\n     Attr: %u", data->partitions[i].attr);
        //}
    }

    if (!found_active) {
        kprintf("\nWait, just ignore everything you just saw lol");

        kfree(data);
        fd_close(fd);
        return;
    }
    char *drive_part_name = kcalloc(strlen(drive_name) + 1 + 2); // px + the null byte
    strcat(drive_part_name, drive_name);
    strcat(drive_part_name, "p");

    for (uint8_t i = 0; i < 4; i++) {
        if (found_active & (1<<i)) {
            mbr_part_offset_t *dev_data = kcalloc(sizeof(mbr_part_offset_t));
            drive_part_name[strlen(drive_part_name) - 1] = (i + '0');
            
            vfs_ops_t ops = dummy_ops;
            ops.open = mbr_open;
            ops.close = mbr_close;
            ops.seek = mbr_seek;
            ops.read = mbr_read;
            ops.write = mbr_write;

            dev_data->drive = kcalloc(strlen(drive_part_name) + 1);
            strcpy(drive_part_name, dev_data->drive);
            dev_data->offset = data->partitions[i].lba_part_start * sector_size;
            dev_data->sector_size = sector_size;
            dev_data->size = data->partitions[i].lba_sector_cnt * sector_size;

            register_device(drive_part_name, ops, dev_data);
        }
    }

    kfree(drive_part_name);
    kfree(data);
    fd_close(fd);
}