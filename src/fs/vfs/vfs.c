#include "vfs.h"
#include "fs/filesystems/filesystems.h"
#include "proc/scheduler.h"
#include "proc/syscalls.h"
#include "klibc/stdlib.h"
#include "klibc/string.h"
#include "klibc/dynarray.h"
#include "klibc/lock.h"
#include "klibc/errno.h"
#include "klibc/open_flags.h"
#include "drivers/tty/tty.h"
#include "drivers/serial.h"
#include "mm/vmm.h"

vfs_node_t *root_node;

lock_t vfs_lock = {0, 0, 0, 0};
uint64_t current_unid = 0; // Current unique node ID

vfs_ops_t null_vfs_ops = {0, 0, 0, 0, 0, 0};

/* Dummy ops */
int dummy_open(char *_1, int _2) {
    (void) _1;
    (void) _2;
    return -ENOSYS;
}

int dummy_post_open(int _1, int _2) {
    (void) _1;
    (void) _2;
    return -ENOSYS;
}

int dummy_close(int _) {
    (void) _;
    return -ENOSYS;
}

int dummy_read(int _1, void *_2, uint64_t _3) {
    (void) _1;
    (void) _2;
    (void) _3;
    return -ENOSYS;
}

int dummy_write(int _1, void *_2, uint64_t _3) {
    (void) _1;
    (void) _2;
    (void) _3;
    return -ENOSYS;
}

uint64_t dummy_seek(int _1, uint64_t _2, int _3) {
    (void) _1;
    (void) _2;
    (void) _3;
    return -ENOSYS;
}

vfs_ops_t dummy_ops = {dummy_open, dummy_post_open, dummy_close, dummy_read, dummy_write, dummy_seek};

static uint8_t search_node_name(vfs_node_t *node, char *name) {
    lock(vfs_lock);
    for (uint64_t i = 0; i < node->children_array_size; i++) {
        vfs_node_t *cur = node->children[i];

        if (cur) {
            if (strcmp(cur->name, name) == 0) {
                unlock(vfs_lock);
                return 1;
            }
        }
    }

    unlock(vfs_lock);
    return 0;
}

vfs_node_t *create_missing_nodes_from_path(char *path, vfs_ops_t ops) {
    vfs_node_t *first_missing = 0;

    char *temp_buffer = kcalloc(strlen(path)); // temporary buffer for each part of the path
    char *current_full_path = kcalloc(strlen(path)); // temporary buffer for all of the path we have parsed
    vfs_node_t *cur_node = (vfs_node_t *) 0;

    current_full_path[0] = '/';

    if (*path == '/') path++;
    if (path[strlen(path) - 1] == '/') path[strlen(path) - 1] = '\0';


    sprintf("[VFS] Parsing path %s\n", path);
    
next_elem:
    for (uint64_t i = 0; *path != '/'; path++) {
        if (*path == '\0') {
            // We are done
            cur_node = get_node_from_path(current_full_path);
            if (!cur_node) {
                kfree(temp_buffer);
                kfree(current_full_path);
                return first_missing;
            }

            // Add the final node
            if (!search_node_name(cur_node, temp_buffer)) {
                vfs_node_t *new_node = vfs_new_node(temp_buffer, ops);
                vfs_add_child(cur_node, new_node);
                if (!first_missing) first_missing = new_node;
            }

            kfree(temp_buffer);
            kfree(current_full_path);
            return first_missing;
        }

        temp_buffer[i++] = *path;
        temp_buffer[i] = '\0';
    }

    cur_node = get_node_from_path(current_full_path);
    if (!cur_node) {
        kfree(temp_buffer);
        kfree(current_full_path);
        return first_missing;
    }

    // Add the next node
    if (!search_node_name(cur_node, temp_buffer)) {
        vfs_node_t *new_node = vfs_new_node(temp_buffer, ops);
        vfs_add_child(cur_node, new_node);
        if (!first_missing) first_missing = new_node;
    }

    path_join(current_full_path, temp_buffer);
    
    path++;
    goto next_elem;
}

void dummy_node_handle(vfs_node_t *_1, vfs_node_t *_2, char *_3) {
    (void) _1;
    (void) _2;
    (void) _3;
}

/* Setting up the root node */
void vfs_init() {
    root_node = kcalloc(sizeof(vfs_node_t));
    root_node->children_array_size = 10;
    root_node->children = kcalloc(10 * sizeof(vfs_node_t *));
    root_node->node_handle = dummy_node_handle; // root node should have a node handler to prevent death
    init_filesystem_handler();
}

/* Creating a new VFS node */
vfs_node_t *vfs_new_node(char *name, vfs_ops_t ops) {
    /* Allocate the space for the new array */
    vfs_node_t *node = kcalloc(sizeof(vfs_node_t));
    node->children_array_size = 10;
    node->children = kcalloc(10 * sizeof(vfs_node_t *));

    /* Set unique node id */
    node->unid = current_unid;
    current_unid++;

    /* Set name and ops */
    node->ops = ops;
    node->name = kcalloc(strlen(name) + 1);
    node->fs_root = 0;
    node->node_handle = 0;
    strcpy(name, node->name);

    sprintf("[VFS] Created new node with name %s\n", node->name);

    return node;
}

/* Add a VFS node as a child of a different VFS node */
void vfs_add_child(vfs_node_t *parent, vfs_node_t *child) {
    lock(vfs_lock);
    uint64_t i = 0;
    for (; i < parent->children_array_size; i++) {
        if (!parent->children[i]) {
            goto fnd; // Found an empty place
        }
    }

    uint64_t old_index = parent->children_array_size;
    parent->children_array_size += 10;
    parent->children = krealloc(parent->children, parent->children_array_size * sizeof(vfs_node_t *));
    parent->children[old_index] = child;
    goto done;
fnd:
    parent->children[i] = child;
done:
    child->parent = parent;
    unlock(vfs_lock);
}

/* Attempt to find a node from a given path */
vfs_node_t *get_node_from_path(char *path) {
    lock(vfs_lock);
    char *buffer = kcalloc(50);
    uint64_t buffer_size = 50;
    uint64_t buffer_index = 0;
    vfs_node_t *cur_node = root_node;

    if (*path++ != '/') { // If the path doesnt start with `/`
        unlock(vfs_lock);
        return (vfs_node_t *) 0;
    }

    while (*path != '\0') {
        if (*path == '/') {
            for (uint64_t i = 0; i < cur_node->children_array_size; i++) {
                if (cur_node->children[i]) {
                    if (strcmp(buffer, cur_node->children[i]->name) == 0) {
                        // Found the next node
                        cur_node = cur_node->children[i]; // Move to the next node
                        buffer_index = 0; // Reset buffer index
                        memset((uint8_t *) buffer, 0, buffer_size); // Clear the buffer
                        goto fnd;
                    }
                }
            }
            
            // No nodes found, return nothing
            unlock(vfs_lock);
            kfree(buffer);
            return (vfs_node_t *) 0;
        } else {
            buffer[buffer_index++] = *path; // Store the data
            if (buffer_index == buffer_size) { // If the buffer is out of bounds, resize it
                buffer_size += 10;
                buffer = krealloc(buffer, buffer_size);
            }
        }
fnd:
        path++;
    }

    if (strlen(buffer) != 0) { // If the buffer hasn't been flushed with a '/'
        for (uint64_t i = 0; i < cur_node->children_array_size; i++) {
            if (cur_node->children[i]) {
                if (strcmp(buffer, cur_node->children[i]->name) == 0) {
                    // Found the next node
                    cur_node = cur_node->children[i]; // Move to the last node
                    goto done;
                }
            }
        }

        // No nodes found, return nothing
        unlock(vfs_lock);
        kfree(buffer);
        return (vfs_node_t *) 0;
    }

done:
    unlock(vfs_lock);
    kfree(buffer);
    return cur_node;
}

void remove_node(vfs_node_t *node) {
    lock(vfs_lock);
    for (uint64_t i = 0; i < node->parent->children_array_size; i++) {
        if (node->parent->children[i] == node) {
            node->parent->children[i] = 0;
        }
    }

    kfree(node);
    unlock(vfs_lock);
}

void set_child_ops(vfs_node_t *node, vfs_ops_t ops) {
    vfs_node_t *current_work = node;

loop:
    for (uint64_t i = 0; i < current_work->children_array_size; i++) {
        if (current_work->children[i] && current_work->children[i]->ops.open != ops.open) {
            current_work = current_work->children[i];
            goto loop;
        }
    }
    if (current_work == node) return;
    vfs_node_t *parent = current_work->parent;
    current_work->ops = ops;
    current_work = parent;
    goto loop;
}

void set_child_fs_root(vfs_node_t *node, vfs_node_t *fs_root) {
    vfs_node_t *current_work = node;

loop:
    for (uint64_t i = 0; i < current_work->children_array_size; i++) {
        if (current_work->children[i] && current_work->children[i]->fs_root != fs_root) {
            current_work = current_work->children[i];
            goto loop;
        }
    }
    if (current_work == node) return;
    vfs_node_t *parent = current_work->parent;
    current_work->fs_root = fs_root;
    current_work = parent;
    goto loop;
}

void remove_children(vfs_node_t *node) {
    vfs_node_t *current_work = node;

loop:
    for (uint64_t i = 0; i < current_work->children_array_size; i++) {
        if (current_work->children[i]) {
            current_work = current_work->children[i];
            goto loop;
        }
    }
    if (current_work == node) return;
    vfs_node_t *parent = current_work->parent;
    remove_node(current_work);
    current_work = parent;
    goto loop;
}

char *get_full_path(vfs_node_t *node) {
    assert(node);

    lock(vfs_lock);
    vfs_node_t *cur_node = node;
    char *path = (char *) 0;
    uint64_t path_index = 0;
    if (cur_node == root_node) {
        path = kcalloc(1);
    }
    while (cur_node != root_node) {
        assert(cur_node);
        assert(cur_node->name);
        path = krealloc(path, path_index + strlen(cur_node->name) + 1);

        for (uint64_t i = strlen(cur_node->name) - 1; i > 0; i--) {
            path[path_index++] = cur_node->name[i];
        }
        path[path_index++] = cur_node->name[0]; // Final char

        // Add the preceding '/'
        path[path_index++] = '/';
        path[path_index] = '\0';
        if (!cur_node->parent) {
            sprintf("bruh\n");
        }
        cur_node = cur_node->parent;
    }

    unlock(vfs_lock);

    reverse(path);
    return path;
}

vfs_node_t *vfs_open(char *name, int mode, uint64_t *err) {
    vfs_node_t *node = get_node_from_path(name);
    if (!node) {
        /* The first missing node, where the generator should start generating */
        vfs_node_t *missing_start = create_missing_nodes_from_path(name, null_vfs_ops);
        assert(missing_start);

        sprintf("[VFS] Handling mountpoint for %s\n", name);
        node = get_node_from_path(name);
        while (node && !node->node_handle) {
            if (node == root_node) {
                remove_children(missing_start);
                remove_node(missing_start);
                *err = ENOENT;
                return (void *) 0; // Welp
            }
            node = node->parent;
        }

        assert(node);
        char *temp_name = name;
        char *full_path = get_full_path(node);
        sprintf("[VFS] Mountpoint at %s\n", full_path);
        temp_name += strlen(full_path);
        kfree(full_path);

        sprintf("[VFS] Temp name: %s\n", temp_name);

        if (mode & O_CREAT) {
            /* The FS will create nodes for us */
            remove_children(missing_start);
            remove_node(missing_start);
            int create_err = node->create_handle(node, temp_name, mode);

            if (create_err != 0) {
                *err = -create_err;
                return NULL;
            } else {
                vfs_node_t *created_file = get_node_from_path(name);
                if (!created_file) {
                    assert(!"vfs go bruh");
                }
                return created_file;
            }
        }

        node->node_handle(node, missing_start, temp_name);
        node = get_node_from_path(name);
        if (!node->ops.open) {
            // No file or directory
            remove_children(missing_start);
            remove_node(missing_start);
            node = 0;
            *err = ENOENT;
        }
    }

    if (node) {
        node->ops.open(name, mode);
    }

    return node;
}

int vfs_read(int fd, void *buf, uint64_t count) {
    vfs_node_t *node = fd_lookup(fd)->node;
    if (!node) {
        sprintf("why is node null lol\n");
        sprintf("fd_cookie1 = %lx\n", fd_lookup(fd)->fd_cookie1);
        sprintf("fd_cookie2 = %lx\n", fd_lookup(fd)->fd_cookie2);
        sprintf("fd_cookie3 = %lx\n", fd_lookup(fd)->fd_cookie3);
        sprintf("fd_cookie4 = %lx\n", fd_lookup(fd)->fd_cookie4);
    }

    // if (strcmp("satadeva", node->name)) {
    //     sprintf("got node %lx with read at %lx and name %s\n", node, node->ops.read, node->name);
    // }
    return node->ops.read(fd, buf, count);
}

int vfs_write(int fd, void *buf, uint64_t count) {
    vfs_node_t *node = fd_lookup(fd)->node;
    return node->ops.write(fd, buf, count);
}

int vfs_close(int fd) {
    vfs_node_t *node = fd_lookup(fd)->node;
    return node->ops.close(fd);
}

uint64_t vfs_seek(int fd, uint64_t offset, int whence) {
    vfs_node_t *node = fd_lookup(fd)->node;
    return node->ops.seek(fd, offset, whence);
}