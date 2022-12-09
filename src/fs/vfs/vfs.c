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
#include "klibc/logger.h"
#include "klibc/debug.h"
#include "drivers/tty/tty.h"
#include "drivers/pit.h"
#include "drivers/serial.h"
#include "mm/vmm.h"
#include "mm/pmm.h"

vfs_node_t *root_node;

lock_t vfs_lock = {0, 0, 0, 0};
lock_t vfs_open_lock = {0, 0, 0, 0}; // Opening files can be a bit hectic on multicore
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

uint8_t search_node_name(vfs_node_t *node, char *name, uint64_t *out) {
    lock(vfs_lock);
    for (uint64_t i = 0; i < node->children_array_size; i++) {
        vfs_node_t *cur = node->children[i];

        if (cur) {
            if (strcmp(cur->name, name) == 0) {
                unlock(vfs_lock);
                if (out) {
                    *out = i;
                }
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
            if (!search_node_name(cur_node, temp_buffer, (void *) 0)) {
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
    if (!search_node_name(cur_node, temp_buffer, (void *) 0)) {
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
    // node->ref_counter = 1; // each node should start with a reference
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

void cleanup_split_path(char **path, uint64_t length) {
    for (uint64_t i = 0; i < length; i++) {
        if (path[i]) {
            kfree(path[i]);
        }
    }

    kfree(path);
}

/* Attempt to find a node from a given path */
vfs_node_t *get_node_from_path(char *path) {
    char **split_list = (void *) 0;
    uint64_t split_list_length = split_path_elements(path, &split_list);

    vfs_node_t *cur_node = root_node;
    for (uint64_t i = 0; i < split_list_length; i++) {
        uint64_t node_index = 0;

        if (!search_node_name(cur_node, split_list[i], &node_index)) {
            cleanup_split_path(split_list, split_list_length);
            return (vfs_node_t *) 0;
        }

        lock(vfs_lock);
        cur_node = cur_node->children[node_index];
        unlock(vfs_lock);
    }

    cleanup_split_path(split_list, split_list_length);
    return cur_node;
}

void remove_node(vfs_node_t *node) {
    lock(vfs_lock);
    // if (atomic_dec(&node->ref_counter)) {
        for (uint64_t i = 0; i < node->parent->children_array_size; i++) {
            if (node->parent->children[i] == node) {
                node->parent->children[i] = 0;
            }
        }

        for (uint64_t i = 0; i < node->children_array_size; i++) {
            if (node->children[i]) {
                remove_node(node->children[i]); // This is recursive so that all node and children get removed.
            }
        }

        kfree(node);
    // }
    // TODO: Add reference counter
    unlock(vfs_lock);
}

char *join_split_path(char **path, uint64_t path_length) {
    char *ret = (void *) 0;
    uint64_t cur_length = 0;

    for (uint64_t i = 0; i < path_length; i++) {
        uint64_t element_length = strlen(path[i]) + 1; // + 1 for the '/'
        cur_length += element_length;

        ret = krealloc(ret, cur_length + 1); // + 1 for the null
        strcat(ret, "/");
        strcat(ret, path[i]);
    }

    return ret;
}

vfs_node_t *get_mountpoint_of_node(vfs_node_t *node) {
    vfs_node_t *cur_node = node;

    lock(vfs_lock);
    while (cur_node != root_node) {
        if (cur_node->filesystem_descriptor) {
            unlock(vfs_lock);
            return cur_node;
        }
        cur_node = cur_node->parent;
    }

    unlock(vfs_lock);
    return cur_node;
}

vfs_node_t *get_mountpoint_of_path(char *path) {
    char **path_elements = (void *) 0;
    uint64_t path_elements_length = split_path_elements(path, &path_elements);

    for (int64_t i = ((int64_t) path_elements_length - 1); i >= 0; i--) {
        char *cur_path = join_split_path(path_elements, i + 1);
        vfs_node_t *node = get_node_from_path(cur_path);
        kfree(cur_path);

        if (node) {
            cleanup_split_path(path_elements, path_elements_length);
            return get_mountpoint_of_node(node);
        }
    }

    cleanup_split_path(path_elements, path_elements_length);
    return root_node;
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

uint64_t split_path_elements(char *path, char ***out) {
    char **output = (void *) 0;
    char *path_ptr = path;
    uint64_t list_length = 0;
    uint64_t cur_string_length = 0;
    uint8_t starting_new = 1;

    if (*path_ptr == '/') path_ptr++;

    while (*path_ptr) {
        if (starting_new && *(path_ptr + 1)) {
            output = krealloc(output, (++list_length) * sizeof(char *));
            output[list_length - 1] = kcalloc(1);

            cur_string_length = 0;
            starting_new = 0;
        }

        if (*path_ptr == '/') {
            assert(!starting_new);
            starting_new = 1;
            path_ptr++;
            continue;
        }

        char *old_string = output[list_length - 1];
        uint64_t old_string_length = cur_string_length++;
        output[list_length - 1] = kcalloc((cur_string_length) + 1); // Allocate for 1 extra for the null, which should also be cleared
        
        memcpy((uint8_t *) old_string, (uint8_t *) output[list_length - 1], old_string_length);
        output[list_length - 1][cur_string_length - 1] = *path_ptr;
        kfree(old_string);

        path_ptr++;
    }

    *out = output;
    return list_length;
}

vfs_node_t *vfs_open(char *name, int mode, uint64_t *err) {
    vfs_node_t *node = (void *) 0;
    (void) mode;
    (void) err;
    // Check if we already have a node
    // If we do, check if the caller wanted to use O_CREAT, if they did, return -EEXIST,
    // Otherwise, return that node.
    // If we do not already have a node:
    // Get the mountpoint of the requested file.
    // Call mountpoint.file_exists(), stripping off the path that belongs to the mountpoint
    // If the file does exist, create nodes in its path that don't exist, HANDLING OPEN ACCORDINGLY!
    //!!!!! <<<<<<<<<<<^^^ HANDLE OPEN DO IT DO IT DO IT
    // Call set_vfs_ops with the filesystem's requested vfs_ops (located in mountpoint.fs_ops)
    // Return the node we have created.

    // char **split_path;
    // uint64_t split_path_length = split_path_elements(name, &split_path);

    return node;
}

int vfs_read(int fd, void *buf, uint64_t count) {
    fd_entry_t *fd_entry = fd_lookup(fd);
    assert(fd_entry);

    vfs_node_t *node = fd_entry->node;
    if (!node) {
        sprintf("why is node null lol\n");
        sprintf("fd_cookie1 = %lx\n", fd_entry->fd_cookie1);
        sprintf("fd_cookie2 = %lx\n", fd_entry->fd_cookie2);
        sprintf("fd_cookie3 = %lx\n", fd_entry->fd_cookie3);
        sprintf("fd_cookie4 = %lx\n", fd_entry->fd_cookie4);
    }

    return node->ops.read(fd, buf, count);
}

int vfs_write(int fd, void *buf, uint64_t count) {
    fd_entry_t *fd_entry = fd_lookup(fd);
    assert(fd_entry);

    vfs_node_t *node = fd_entry->node;
    return node->ops.write(fd, buf, count);
}

int vfs_close(int fd) {
    fd_entry_t *fd_entry = fd_lookup(fd);
    assert(fd_entry);

    vfs_node_t *node = fd_entry->node;
    return node->ops.close(fd);
}

uint64_t vfs_seek(int fd, uint64_t offset, int whence) {
    fd_entry_t *fd_entry = fd_lookup(fd);
    assert(fd_entry);

    vfs_node_t *node = fd_entry->node;
    return node->ops.seek(fd, offset, whence);
}