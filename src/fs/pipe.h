#ifndef PIPE_H
#define PIPE_H
#include <stdint.h>
#include "proc/event.h"
#include "fs/vfs/vfs.h"

#define PIPE_DEFAULT_BUFFER_SIZE 4096
#define PIPE_DIRECTION_TO_TARGET 1
#define PIPE_DIRECTION_FROM_TARGET 2

typedef struct {
    int creator_pid;
    int target_pid;
    int creator_fd;
    int target_fd;

    uint64_t buffer_size;
    uint8_t *read_pointer;
    uint8_t *write_pointer;
    uint8_t *buffer;
    event_t write_event;
    uint8_t direction;
} pipe_t;

extern vfs_node_t pipe_node;

void init_pipe();
void create_pipe(int cur_fd, int remote_fd, int other_pid, int direction);

#endif