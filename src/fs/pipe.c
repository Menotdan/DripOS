#include "pipe.h"
#include "klibc/stdlib.h"
#include "klibc/errno.h"
#include "sys/smp.h"
#include "fd.h"
#include <stddef.h>

uint64_t pipes_size = 0;
pipe_t **pipes = NULL;
lock_t pipe_lock = {0, 0, 0, 0};

void create_pipe(int cur_fd, int remote_fd, int other_pid, int direction) {
    lock(pipe_lock);
    int64_t index = -1;
    for (uint64_t i = 0; i < pipes_size; i++) {
        if (!pipes[i]) {
            index = i;
            break;
        }
    }

    if (index == -1) {
        krealloc(pipes, sizeof(pipe_t *) * (pipes_size + 1));
        index = pipes_size++;
    }

    pipes[index] = kcalloc(sizeof(pipe_t));
    pipes[index]->buffer = kcalloc(PIPE_DEFAULT_BUFFER_SIZE);
    pipes[index]->buffer_size = PIPE_DEFAULT_BUFFER_SIZE;
    pipes[index]->creator_fd = cur_fd;
    pipes[index]->creator_pid = get_cpu_locals()->current_thread->parent_pid;
    pipes[index]->target_pid = other_pid;
    pipes[index]->target_fd = remote_fd;
    pipes[index]->direction = direction;
    pipes[index]->write_pointer = pipes[index]->read_pointer = pipes[index]->buffer;
    unlock(pipe_lock);
}

pipe_t *lookup_pipe(int fd) {
    lock(pipe_lock);
    if (pipes != NULL) {
        pipe_t *pipe;
        for (uint64_t i = 0; i < pipes_size; i++) {
            pipe = pipes[i];
            if ((fd == pipe->creator_fd && get_cpu_locals()->current_thread->parent_pid == pipe->creator_pid) || (fd == pipe->target_fd && get_cpu_locals()->current_thread->parent_pid == pipe->target_pid)) {
                unlock(pipe_lock);
                return pipe;
            }
        }
        unlock(pipe_lock);
        return NULL;
    } else {
        unlock(pipe_lock);
        return NULL;
    }
}

int pipe_read(int fd, void *buf, uint64_t count) {
    pipe_t *pipe = lookup_pipe(fd);
    if (pipe == NULL) {
        return -EBADF;
    }

    if (pipe->creator_fd == fd && pipe->creator_pid == get_cpu_locals()->current_thread->parent_pid && pipe->direction != PIPE_DIRECTION_FROM_TARGET) {
        return -EPERM;
    }

    if (pipe->target_fd == fd && pipe->target_pid == get_cpu_locals()->current_thread->parent_pid && pipe->direction != PIPE_DIRECTION_TO_TARGET) {
        return -EPERM;
    }

    lock(pipe_lock);
    if (pipe->read_pointer == pipe->write_pointer) {
        unlock(pipe_lock);
        await_event(&pipe->write_event);
        lock(pipe_lock);
    }

    uint64_t pointer_difference = pipe->write_pointer - pipe->read_pointer;
    if (count < pointer_difference) {
        pointer_difference = count;
    }

    memcpy(pipe->read_pointer, buf, pointer_difference);

    if (pipe->read_pointer + pointer_difference == pipe->write_pointer) { // If we read all the data...
        pipe->read_pointer = pipe->write_pointer = pipe->buffer; // Reset the pipe buffers, since we finished reading
    }
    pipe->write_event = 0;
    unlock(pipe_lock);

    return pointer_difference;
}

int pipe_write(int fd, void *buf, uint64_t count) {
    pipe_t *pipe = lookup_pipe(fd);
    if (pipe == NULL) {
        return -EBADF;
    }

    if (pipe->creator_fd == fd && pipe->creator_pid == get_cpu_locals()->current_thread->parent_pid && pipe->direction != PIPE_DIRECTION_TO_TARGET) {
        return -EPERM;
    }

    if (pipe->target_fd == fd && pipe->target_pid == get_cpu_locals()->current_thread->parent_pid && pipe->direction != PIPE_DIRECTION_FROM_TARGET) {
        return -EPERM;
    }

    lock(pipe_lock);
    if (pipe->buffer_size - (pipe->write_pointer - pipe->buffer) < count) {
        uint64_t new_bytes = (((count - (pipe->buffer_size - (pipe->write_pointer - pipe->buffer))) + 1) + 0x1000 - 1) / 0x1000;
        uint64_t write_offset = pipe->write_pointer - pipe->buffer;
        uint64_t read_offset = pipe->read_pointer - pipe->buffer;

        pipe->buffer = krealloc(pipe->buffer, pipe->buffer_size + new_bytes);
        pipe->buffer_size += new_bytes;
        pipe->write_pointer = pipe->buffer + write_offset;
        pipe->read_pointer = pipe->buffer + read_offset;
    }

    memcpy(buf, pipe->write_pointer, count);
    pipe->write_pointer += count;
    trigger_event(&pipe->write_event);

    unlock(pipe_lock);

    return count;
}

vfs_ops_t pipe_ops;
vfs_node_t pipe_node;

void init_pipe() {
    memset((uint8_t *) &pipe_node, 0, sizeof(vfs_node_t));
    pipe_ops = dummy_ops;
    pipe_ops.read = pipe_read;
    pipe_ops.write = pipe_write;
    pipe_node.ops = pipe_ops;
    pipe_node.name = "pipe";
}