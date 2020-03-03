#include "fd.h"
#include "klibc/lock.h"
#include "klibc/stdlib.h"
#include "proc/scheduler.h"

#include "drivers/serial.h"

vfs_node_t **fd_table;
int fd_table_size = 0;
lock_t fd_lock = 0;

int fd_open(char *name, int mode) {
    vfs_node_t *node = vfs_open(name, mode);
    if (!node) {
        return -1;
    }
    return fd_new(node);
}

int fd_close(int fd) {
    vfs_node_t *node = fd_lookup(fd);
    if (!node) {
        return -1;
    }
    vfs_close(node);
    fd_remove(fd);
    return get_thread_locals()->errno;
}

int fd_read(int fd, void *buf, uint64_t count) {
    vfs_node_t *node = fd_lookup(fd);
    if (!node) {
        return -1;
    }
    vfs_read(node, buf, count);
    return get_thread_locals()->errno;
}

int fd_write(int fd, void *buf, uint64_t count) {
    vfs_node_t *node = fd_lookup(fd);
    if (!node) {
        return -1;
    }
    sprintf("\nPerforming a write");
    vfs_write(node, buf, count);
    sprintf("\nDone");
    return get_thread_locals()->errno;
}

int fd_new(vfs_node_t *node) {
    lock(&fd_lock);
    int i = 0;
    for (; i < fd_table_size; i++) {
        if (!fd_table[i]) {
            // Found an empty space
            goto fnd;
        }
    }
    i = fd_table_size; // Get the old table size
    fd_table_size += 10;
    fd_table = krealloc(fd_table, fd_table_size * sizeof(vfs_node_t *));
fnd:
    fd_table[i] = node;
    unlock(&fd_lock);
    return i;
}

void fd_remove(int fd) {
    lock(&fd_lock);
    
    if (fd < fd_table_size - 1) {
        fd_table[fd] = (vfs_node_t *) 0;
    }

    unlock(&fd_lock);
}

vfs_node_t *fd_lookup(int fd) {
    vfs_node_t *ret;
    lock(&fd_lock);
    if (fd < fd_table_size - 1) {
        ret = fd_table[fd];
    } else {
        ret = (vfs_node_t *) 0;
    }
    unlock(&fd_lock);
    return ret;
}