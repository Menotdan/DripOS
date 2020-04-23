#include "fd.h"
#include "klibc/lock.h"
#include "klibc/stdlib.h"
#include "klibc/errno.h"
#include "sys/smp.h"
#include "proc/scheduler.h"

#include "drivers/serial.h"

lock_t fd_lock = {0, 0, 0};

int fd_open(char *name, int mode) {
    vfs_node_t *node = vfs_open(name, mode);
    if (!node) {
        return get_thread_locals()->errno;
    }
    return fd_new(node, mode, get_cpu_locals()->current_thread->parent_pid);
}

int open_remote_fd(char *name, int mode, int pid) {
    vfs_node_t *node = vfs_open(name, mode);
    if (!node) {
        return get_thread_locals()->errno;
    }
    return fd_new(node, mode, pid);
}

int fd_close(int fd) {
    fd_entry_t *node = fd_lookup(fd);
    if (!node) {
        get_thread_locals()->errno = -EBADF;
        return get_thread_locals()->errno;
    }
    int ret = vfs_close(fd);
    fd_remove(fd);
    return ret;
}

int fd_read(int fd, void *buf, uint64_t count) {
    fd_entry_t *node = fd_lookup(fd);
    if (!node) {
        get_thread_locals()->errno = -EBADF;
        return get_thread_locals()->errno;
    }
    return vfs_read(fd, buf, count);
}

int fd_write(int fd, void *buf, uint64_t count) {
    fd_entry_t *node = fd_lookup(fd);
    if (!node) {
        get_thread_locals()->errno = -EBADF;
        return get_thread_locals()->errno;
    }
    return vfs_write(fd, buf, count);
}

int fd_seek(int fd, uint64_t offset, int whence) {
    fd_entry_t *node = fd_lookup(fd);
    if (!node) {
        get_thread_locals()->errno = -EBADF;
        return get_thread_locals()->errno;
    }

    if (whence == 0) {
        node->seek  = offset;
    } else if (whence == 1) {
        node->seek += offset;
    } else if (whence == 2) {
        node->seek -= offset;
    }

    return vfs_seek(fd, offset, whence);
}

int fd_new(vfs_node_t *node, int mode, int pid) {
    lock(fd_lock);
    process_t *current_process = reference_process(pid);
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
    sprintf("\nFd new: %d", i);
    sprintf("\nFd table: %lx", fd_table);
    unlock(fd_lock);
    return i;
}

void fd_remove(int fd) {
    lock(fd_lock);
    process_t *current_process = reference_process(get_cpu_locals()->current_thread->parent_pid);
    fd_entry_t **fd_table = current_process->fd_table;
    int *fd_table_size = &current_process->fd_table_size;
    
    if (fd < *fd_table_size - 1) {
        kfree(fd_table[fd]);
        sprintf("\nFd removed: %d", fd);
        sprintf("\nFd table: %lx", fd_table);
        fd_table[fd] = (fd_entry_t *) 0;
    }

    unlock(fd_lock);
}

fd_entry_t *fd_lookup(int fd) {
    fd_entry_t *ret;
    lock(fd_lock);
    process_t *current_process = reference_process(get_cpu_locals()->current_thread->parent_pid);
    fd_entry_t **fd_table = current_process->fd_table;
    int *fd_table_size = &current_process->fd_table_size;

    sprintf("\nFd lookup: %d", fd);
    sprintf("\nFd table: %lx", &fd_table[fd]);
    if (fd < *fd_table_size - 1 && fd >= 0) {
        ret = fd_table[fd];
    } else {
        sprintf("\nBad fd: %d", fd);
        ret = (fd_entry_t *) 0;
    }
    unlock(fd_lock);
    return ret;
}