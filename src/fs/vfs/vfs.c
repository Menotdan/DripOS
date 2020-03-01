#include "vfs.h"
#include "klibc/stdlib.h"
#include "klibc/string.h"
#include "klibc/dynarray.h"
#include "klibc/lock.h"
#include "klibc/errno.h"
#include "drivers/tty/tty.h"
#include "drivers/serial.h"

vfs_node_t *root_node;

lock_t vfs_lock;

/* Dummy VFS ops */
int dummy_open(char *name, int mode) {
    (void) name;
    (void) mode;
    return -ENOSYS;
}

int dummy_close(vfs_node_t *node) {
    (void) node;
    return -ENOSYS;
}

int dummy_read(vfs_node_t *node, void *buf, uint64_t bytes) {
    (void) node;
    (void) buf;
    (void) bytes;
    return -ENOSYS;
}

int dummy_write(vfs_node_t *node, void *buf, uint64_t bytes) {
    (void) node;
    (void) buf;
    (void) bytes;
    return -ENOSYS;
}

vfs_ops_t dummy_ops = {dummy_open, dummy_close, dummy_read, dummy_write};

/* Setting up the root node */
void vfs_init() {
    root_node = kcalloc(sizeof(vfs_node_t));
    root_node->children_array_size = 10;
    root_node->children = kcalloc(10 * sizeof(vfs_node_t *));
}

/* Creating a new VFS node */
vfs_node_t *new_node(char *name, vfs_ops_t ops) {
    /* Allocate the space for the new array */
    vfs_node_t *node = kcalloc(sizeof(vfs_node_t));
    node->children_array_size = 10;
    node->children = kcalloc(10 * sizeof(vfs_node_t *));

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
            sprintf("\nBuffer: %s", buffer);
            for (uint64_t i = 0; i < cur_node->children_array_size; i++) {
                if (strcmp(buffer, cur_node->children[i]->name) == 0) {
                    // Found the next node
                    cur_node = cur_node->children[i]; // Move to the next node
                    buffer_index = 0; // Reset buffer index
                    memset((uint8_t *) buffer, 0, buffer_size); // Clear the buffer
                    goto fnd;
                }
            }
            unlock(&vfs_lock);
            return (vfs_node_t *) 0; // Found nothing
        } else {
            buffer[buffer_index++] = *path; // Store the data
            if (buffer_index == buffer_size) { // If the buffer is out of bounds, resize it
                buffer_size += 10;
                krealloc(buffer, buffer_size);
            }
        }
fnd:
        path++;
    }

    unlock(&vfs_lock);
    return cur_node;
}

void vfs_test() {
    vfs_node_t *node1 = new_node("test", dummy_ops);
    vfs_add_child(root_node, node1);
    vfs_node_t *node2 = new_node("test1", dummy_ops);
    vfs_add_child(node1, node2);

    vfs_node_t *node2_from_array = get_node_from_path("/test/test1/");
    sprintf("\nNode: %lx", node2_from_array);
    sprintf("\nNode name: %s", node2_from_array->name);
}