#include "vfs.h"
#include "proc/scheduler.h"
#include "klibc/stdlib.h"
#include "klibc/string.h"
#include "klibc/dynarray.h"
#include "klibc/lock.h"
#include "klibc/errno.h"
#include "drivers/tty/tty.h"
#include "drivers/serial.h"
#include "mm/vmm.h"

vfs_node_t *root_node;

lock_t vfs_lock = 0;
uint64_t current_unid = 0; // Current unique node ID

/* Dummy VFS ops */
int dummy_open(char *name, int mode) {
    (void) name;
    (void) mode;
    get_thread_locals()->errno = -ENOSYS;
    return -1;
}

int dummy_close(fd_entry_t *node) {
    (void) node;
    get_thread_locals()->errno = -ENOSYS;
    return -1;
}

int dummy_read(fd_entry_t *node, void *buf, uint64_t bytes) {
    (void) node;
    (void) buf;
    (void) bytes;
    get_thread_locals()->errno = -ENOSYS;
    return -1;
}

int dummy_write(fd_entry_t *node, void *buf, uint64_t bytes) {
    (void) node;
    (void) buf;
    (void) bytes;
    get_thread_locals()->errno = -ENOSYS;
    return -1;
}

vfs_ops_t dummy_ops = {dummy_open, dummy_close, dummy_read, dummy_write};

/* VFS ops things */
vfs_node_t *vfs_open(char *name, int mode) {
    if (!range_mapped(name, 0x1000)) { // boi if ur name isnt mapped
        get_thread_locals()->errno = -EFAULT;
        sprintf("\nName not mapped in vfs_open");
        return (vfs_node_t *) 0;
    }

    vfs_node_t *node = get_node_from_path(name);
    if (node) {
        /* Call the actual open ops */
        node->ops.open(name, mode);
    } else {
        get_thread_locals()->errno = -ENOENT; // Set errno and we will then return null
    }

    return node;
}

void vfs_close(fd_entry_t *node) {
    /* If no mappings exist */
    if (!range_mapped(node, sizeof(vfs_node_t))) {
        sprintf("\nNode not mapped in vfs_close");
        get_thread_locals()->errno = -EFAULT;
        return;
    }

    node->node->ops.close(node);
}

void vfs_read(fd_entry_t *node, void *buf, uint64_t count) {
    /* If no mappings exist */
    if (!range_mapped(node, sizeof(vfs_node_t)) || !range_mapped(buf, count)) {
        sprintf("\nNode/buffer not mapped in vfs_read");
        get_thread_locals()->errno = -EFAULT;
        return;
    }

    node->node->ops.read(node, buf, count);
}

void vfs_write(fd_entry_t *node, void *buf, uint64_t count) {
    /* If no mappings exist */
    if (!range_mapped(node, sizeof(vfs_node_t)) || !range_mapped(buf, count)) {
        sprintf("\nNode/buffer not mapped in vfs_write");
        get_thread_locals()->errno = -EFAULT;
        return;
    }

    node->node->ops.write(node, buf, count);
}

/* Setting up the root node */
void vfs_init() {
    root_node = kcalloc(sizeof(vfs_node_t));
    root_node->children_array_size = 10;
    root_node->children = kcalloc(10 * sizeof(vfs_node_t *));
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
    strcpy(name, node->name);

    return node;
}

/* Add a VFS node as a child of a different VFS node */
void vfs_add_child(vfs_node_t *parent, vfs_node_t *child) {
    lock(&vfs_lock);
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
    unlock(&vfs_lock);
}

/* Attempt to find a node from a given path */
vfs_node_t *get_node_from_path(char *path) {
    lock(&vfs_lock);
    char *buffer = kcalloc(50);
    uint64_t buffer_size = 50;
    uint64_t buffer_index = 0;
    vfs_node_t *cur_node = root_node;

    if (*path++ != '/') { // If the path doesnt start with `/`
        unlock(&vfs_lock);
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
            unlock(&vfs_lock);
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
        unlock(&vfs_lock);
        kfree(buffer);
        return (vfs_node_t *) 0;
    }

done:
    unlock(&vfs_lock);
    kfree(buffer);
    return cur_node;
}