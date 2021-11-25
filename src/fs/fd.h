#ifndef FS_FD_H
#define FS_FD_H
#include <stdint.h>
#include "vfs/vfs.h"
#include "klibc/lock.h"

#define FD_COOKIE_VAL 0x1111222233334444

#define SEEK_CUR 1
#define SEEK_END 2
#define SEEK_SET 3

typedef struct fd_entry {
    uint64_t fd_cookie1;
    vfs_node_t *node;
    uint64_t fd_cookie2;
    uint64_t seek;
    uint64_t fd_cookie3;
    int mode;
    uint64_t fd_cookie4;
} fd_entry_t;

extern lock_t fd_lock;

struct vfs_node;
typedef struct vfs_node vfs_node_t;

int fd_new(vfs_node_t *node, int mode, int pid);
void fd_remove(int fd);
void fd_remove_pid(int fd, int pid);
fd_entry_t *fd_lookup(int fd);

/* FD ops */
int fd_open(char *name, int mode);
int fd_close(int fd);
int fd_read(int fd, void *buf, uint64_t count);
int fd_write(int fd, void *buf, uint64_t count);
uint64_t fd_seek(int fd, uint64_t offset, int whence);

int open_remote_fd(char *name, int mode, int pid);

void clone_fds(int64_t old_pid, int64_t new_pid);

#endif