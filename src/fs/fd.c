#include "fd.h"
#include "klibc/lock.h"
#include "klibc/stdlib.h"
#include "klibc/errno.h"
#include "mm/vmm.h"
#include "sys/smp.h"
#include "proc/scheduler.h"
#include "proc/safe_userspace.h"

#include "drivers/serial.h"

lock_t fd_lock = {0, 0, 0, 0};

int fd_open(char *filepath, int mode) {
    char *kernel_string = check_and_copy_string(filepath);
    if (!kernel_string) { // if err
        return -EFAULT;
    }

    int set_ignore = 0;
    if (get_cpu_locals()->current_thread->ring == 3 && !get_cpu_locals()->ignore_ring) {
        get_cpu_locals()->ignore_ring = 1;
        set_ignore = 1;
    }

    uint64_t err;
    vfs_node_t *node = vfs_open(kernel_string, mode, &err);
    kfree(kernel_string);
    if (!node) {
        if (set_ignore)
            get_cpu_locals()->ignore_ring = 0;
        return -err;
    }

    int new_fd = fd_new(node, mode, get_cpu_locals()->current_thread->parent_pid);
    if (set_ignore)
        get_cpu_locals()->ignore_ring = 0;
    return new_fd;
}

int open_remote_fd(char *name, int mode, int pid) {
    uint64_t err;
    vfs_node_t *node = vfs_open(name, mode, &err);
    if (!node) {
        return err;
    }

    return fd_new(node, mode, pid);
}

int fd_close(int fd) {
    fd_entry_t *node = fd_lookup(fd);
    if (!node) {
        return -EBADF;
    }
    int ret = vfs_close(fd);
    fd_remove(fd);
    return ret;
}

int fd_read(int fd, void *buf, uint64_t count) {
    int set_ignore = 0;
    if (get_cpu_locals()->current_thread->ring == 3 && !get_cpu_locals()->ignore_ring) {
        get_cpu_locals()->ignore_ring = 1;
        set_ignore = 1;
    }

    fd_entry_t *node = fd_lookup(fd);
    if (!node) {
        if (set_ignore)
            get_cpu_locals()->ignore_ring = 0;
        return -EBADF;
    }

    if (!range_mapped(buf, count)) {
        if (set_ignore)
            get_cpu_locals()->ignore_ring = 0;
        return -EBADF;
    }

    //sprintf("got to vfs_read\n");
    int read = vfs_read(fd, buf, count);
    if (set_ignore)
        get_cpu_locals()->ignore_ring = 0;
    //sprintf("got past vfs_read\n");
    return read;
}

int fd_write(int fd, void *buf, uint64_t count) {
    int set_ignore = 0;
    if (get_cpu_locals()->current_thread->ring == 3 && !get_cpu_locals()->ignore_ring) {
        get_cpu_locals()->ignore_ring = 1;
        set_ignore = 1;
    }

    fd_entry_t *node = fd_lookup(fd);
    if (!node) {
        if (set_ignore)
            get_cpu_locals()->ignore_ring = 0;
        return -EBADF;
    }

    if (!range_mapped(buf, count)) {
        if (set_ignore)
            get_cpu_locals()->ignore_ring = 0;
        return -EFAULT;
    }

    int ret = vfs_write(fd, buf, count);
    if (set_ignore)
        get_cpu_locals()->ignore_ring = 0;
    return ret;
}

uint64_t fd_seek(int fd, uint64_t offset, int whence) {
    int set_ignore = 0;
    if (get_cpu_locals()->current_thread->ring == 3 && !get_cpu_locals()->ignore_ring) {
        get_cpu_locals()->ignore_ring = 1;
        set_ignore = 1;
    }

    fd_entry_t *node = fd_lookup(fd);
    if (!node) {
        if (set_ignore)
            get_cpu_locals()->ignore_ring = 0;
        return -EBADF;
    }

    if (whence == SEEK_SET) {
        node->seek = offset;
    } else if (whence == SEEK_CUR) {
        node->seek += offset;
    } else if (whence == SEEK_END) {
    } else {
        if (set_ignore)
            get_cpu_locals()->ignore_ring = 0;
        return -EINVAL;
    }

    uint64_t ret = vfs_seek(fd, offset, whence);
    if (set_ignore)
            get_cpu_locals()->ignore_ring = 0;

    if (!ret) {
        return node->seek;
    }
    return ret;
}

int fd_new(vfs_node_t *node, int mode, int pid) {
    interrupt_safe_lock(sched_lock);
    if ((uint64_t) pid >= process_list_size) {
        sprintf("ERRRRRRRRRRRRRROR in fd_new: pid bad: %d\n", pid);
    }
    process_t *current_process = processes[pid];
    interrupt_safe_unlock(sched_lock);

    lock(fd_lock);
    fd_entry_t **fd_table = current_process->fd_table;
    int *fd_table_size = &current_process->fd_table_size;

    fd_entry_t *new_entry = kcalloc(sizeof(fd_entry_t));
    new_entry->node = node;
    new_entry->mode = mode;
    new_entry->seek = 0;

    int i = 0;
    for (; i < *fd_table_size; i++) {
        if (!fd_table[i]) {
            // Found an empty space
            goto fnd;
        }
    }
    i = *fd_table_size; // Get the old table size
    *fd_table_size += 10;
    fd_table = krealloc(fd_table, *fd_table_size * sizeof(fd_entry_t *));
fnd:
    fd_table[i] = new_entry;
    unlock(fd_lock);

    return i;
}

void fd_remove(int fd) {
    interrupt_safe_lock(sched_lock);
    process_t *current_process = processes[get_cpu_locals()->current_thread->parent_pid];
    interrupt_safe_unlock(sched_lock);

    lock(fd_lock);
    fd_entry_t **fd_table = current_process->fd_table;
    int *fd_table_size = &current_process->fd_table_size;
    
    if (fd < *fd_table_size - 1) {
        kfree(fd_table[fd]);
        fd_table[fd] = (fd_entry_t *) 0;
    }

    unlock(fd_lock);
}

fd_entry_t *fd_lookup(int fd) {
    fd_entry_t *ret;

    interrupt_safe_lock(sched_lock);
    process_t *current_process = processes[get_cpu_locals()->current_thread->parent_pid];
    interrupt_safe_unlock(sched_lock);

    lock(fd_lock);
    fd_entry_t **fd_table = current_process->fd_table;
    int *fd_table_size = &current_process->fd_table_size;

    if (fd < *fd_table_size - 1 && fd >= 0) {
        ret = fd_table[fd];
    } else {
        ret = (fd_entry_t *) 0;
    }
    unlock(fd_lock);

    return ret;
}

void clone_fds(int64_t old_pid, int64_t new_pid) {
    interrupt_safe_lock(sched_lock);
    if ((uint64_t) old_pid >= process_list_size || (uint64_t) new_pid >= process_list_size) {
        sprintf("BAD ERROR REEEEEEEEEEEEEEEEEEEEEE (go look in clone_fds) old_pid: %ld, new_pid: %ld\n", old_pid, new_pid);
    }
    process_t *old = processes[old_pid];
    process_t *new = processes[new_pid];
    interrupt_safe_unlock(sched_lock);


    lock(fd_lock);
    for (int i = 0; i < old->fd_table_size; i++) {
        if (old->fd_table[i]) {
            fd_entry_t *new_fd = kcalloc(sizeof(fd_entry_t));
            new_fd->node = old->fd_table[i]->node;
            new_fd->mode = old->fd_table[i]->mode;
            new_fd->seek = old->fd_table[i]->seek;

            int i = 0;
            for (; i < new->fd_table_size; i++) {
                if (!new->fd_table[i]) {
                    // Found an empty space
                    goto fnd;
                }
            }
            i = new->fd_table_size; // Get the old table size
            new->fd_table_size += 10;
            new->fd_table = krealloc(new->fd_table, new->fd_table_size * sizeof(fd_entry_t *));
        fnd:
            new->fd_table[i] = new_fd;
        }
    }
    unlock(fd_lock);
}