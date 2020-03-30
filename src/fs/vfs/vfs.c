#include "vfs.h"
#include "fs/filesystems/filesystems.h"
#include "proc/scheduler.h"
#include "proc/syscalls.h"
#include "klibc/stdlib.h"
#include "klibc/string.h"
#include "klibc/dynarray.h"
#include "klibc/lock.h"
#include "klibc/errno.h"
#include "drivers/tty/tty.h"
#include "drivers/serial.h"
#include "mm/vmm.h"

vfs_node_t *root_node;

lock_t vfs_lock = {0, 0, 0};
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

int dummy_seek(fd_entry_t *node, uint64_t offset, int whence) {
    (void) node;
    (void) offset;
    (void) whence;
    get_thread_locals()->errno = -ENOSYS;
    return -1;
}

vfs_ops_t dummy_ops = {dummy_open, dummy_close, dummy_read, dummy_write, dummy_seek};

/* some nonsense string handling and stuff for getting a mountpoint ig */
void attempt_mountpoint_handle(char *path) {
    char *local_path = kcalloc(strlen(path) + 1);
    char *filesystem_path = kcalloc(strlen(path) + 1);
    strcpy(path, local_path);

    sprintf("\n[VFS] Start path: ");
    sprint(local_path);

    while (1) {
        sprintf("\n[VFS] Path: ");
        sprint(local_path);
        sprintf("\n[VFS] Other path: ");
        sprint(filesystem_path);
        if (is_mountpoint(local_path)) {
            reverse(filesystem_path);
            sprintf("\n[VFS] Doing mountpoint handle for ");
            sprint(filesystem_path);
            gen_node_mountpoint(local_path, filesystem_path);

            return;
        }
        char *tmp_get_path = local_path + strlen(local_path) - 1;
        while (*tmp_get_path != '/')
            *(filesystem_path + strlen(filesystem_path)) = *tmp_get_path--;
        // Add the '/'
        *(filesystem_path + strlen(filesystem_path)) = *tmp_get_path;

        *tmp_get_path = '\0';
        if (*local_path == '\0') {
            break;
        }
    }

    // Handle '/' as a mountpoint
    sprintf("\n[VFS] Path: ");
    sprint(local_path);
    sprintf("\n[VFS] Other path: ");
    sprint(filesystem_path);
    if (is_mountpoint("/")) {
        reverse(filesystem_path);
        sprintf("\n[VFS] Doing mountpoint handle for ");
        sprint(filesystem_path);
        gen_node_mountpoint(local_path, filesystem_path);

        return;
    }

    sprintf("\nDoneee");

    kfree(local_path);
    kfree(filesystem_path);
}

uint8_t search_node_name(vfs_node_t *node, char *name) {
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

void create_missing_nodes_from_path(char *path, vfs_ops_t ops, vfs_node_t *mountpoint) {
    char *temp_buffer = kcalloc(strlen(path)); // temporary buffer for each part of the path
    char *current_full_path = kcalloc(strlen(path)); // temporary buffer for all of the path we have parsed
    vfs_node_t *cur_node = (vfs_node_t *) 0;

    current_full_path[0] = '/';

    if (*path == '/') path++;
    if (path[strlen(path) - 1] == '/') path[strlen(path) - 1] = '\0';


    sprintf("\n[VFS] Parsing path ");
    sprint(path);
    
next_elem:
    for (uint64_t i = 0; *path != '/'; path++) {
        if (*path == '\0') {
            // We are done
            cur_node = get_node_from_path(current_full_path);
            if (!cur_node) {
                kfree(temp_buffer);
                kfree(current_full_path);
                return;
            }

            // Add the final node
            if (!search_node_name(cur_node, temp_buffer)) {
                vfs_node_t *new_node = vfs_new_node(temp_buffer, ops);
                new_node->mountpoint = mountpoint;
                vfs_add_child(cur_node, new_node);
            }

            kfree(temp_buffer);
            kfree(current_full_path);
            return;
        }

        temp_buffer[i++] = *path;
        temp_buffer[i] = '\0';
    }

    cur_node = get_node_from_path(current_full_path);
    if (!cur_node) {
        kfree(temp_buffer);
        kfree(current_full_path);
        return;
    }

    // Add the next node
    if (!search_node_name(cur_node, temp_buffer)) {
        vfs_node_t *new_node = vfs_new_node(temp_buffer, ops);
        new_node->mountpoint = mountpoint;
        vfs_add_child(cur_node, new_node);
    }

    path_join(current_full_path, temp_buffer);
    
    path++;
    goto next_elem;
}

/* VFS ops things */
vfs_node_t *vfs_open(char *input, int mode) {
    char *name = kcalloc(4098);
    uint64_t data_count = strcpy_from_userspace(name, input);

    if (data_count == 0xFFFF || data_count == 0xFFFFF) { // Name too big or PF
        get_thread_locals()->errno = -EFAULT;
        sprintf("\nName not mapped in vfs_open");

        kfree(name);
        return (vfs_node_t *) 0;
    }

    if (*name != '/') {
        kfree(name);
        get_thread_locals()->errno = -ENOENT;
        return (vfs_node_t *) 0;
    }

    if (name[strlen(name) - 1] == '/' && strlen(name) != 1) name[strlen(name) - 1] = '\0'; // Remove the last '/'

    vfs_node_t *node = get_node_from_path(name);
    if (node) {
        /* Call the actual open ops */
        node->ops.open(name, mode);
    } else {
        sprintf("\nAttempting mountpoint handle");
        attempt_mountpoint_handle(name);
        node = get_node_from_path(name);
        if (node) {
            node->ops.open(name, mode);
        } else {
            get_thread_locals()->errno = -ENOENT; // Set errno and we will then return null
        }
    }

    kfree(name);
    return node;
}

int vfs_close(fd_entry_t *node) {
    /* If no mappings exist */
    if (!range_mapped(node, sizeof(vfs_node_t))) {
        sprintf("\nNode not mapped in vfs_close");
        get_thread_locals()->errno = -EFAULT;
        return -1;
    }

    return node->node->ops.close(node);
}

int vfs_read(fd_entry_t *node, void *buf, uint64_t count) {
    /* If no mappings exist */
    if (!range_mapped(node, sizeof(vfs_node_t)) || !range_mapped(buf, count)) {
        sprintf("\nNode/buffer not mapped in vfs_read");
        get_thread_locals()->errno = -EFAULT;
        return -1;
    }

    return node->node->ops.read(node, buf, count);
}

int vfs_write(fd_entry_t *node, void *buf, uint64_t count) {
    /* If no mappings exist */
    if (!range_mapped(node, sizeof(vfs_node_t)) || !range_mapped(buf, count)) {
        sprintf("\nNode/buffer not mapped in vfs_write");
        get_thread_locals()->errno = -EFAULT;
        return -1;
    }

    return node->node->ops.write(node, buf, count);
}

int vfs_seek(fd_entry_t *node, uint64_t offset, int whence) {
    /* If no mappings exist */
    if (!range_mapped(node, sizeof(vfs_node_t))) {
        sprintf("\nNode not mapped in vfs_close");
        get_thread_locals()->errno = -EFAULT;
        return 0;
    }

    return node->node->ops.seek(node, offset, whence);
}

/* Setting up the root node */
void vfs_init() {
    root_node = kcalloc(sizeof(vfs_node_t));
    root_node->children_array_size = 10;
    root_node->children = kcalloc(10 * sizeof(vfs_node_t *));
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
    strcpy(name, node->name);

    sprintf("\n[VFS] Created new node with name ");
    sprint(node->name);

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

char *get_full_path(vfs_node_t *node) {
    lock(vfs_lock);
    vfs_node_t *cur_node = node;
    char *path = (char *) 0;
    uint64_t path_index = 0;
    while (cur_node != root_node) {
        path = krealloc(path, path_index + strlen(cur_node->name) + 1);
        for (uint64_t i = strlen(cur_node->name) - 1; i > 0; i--) {
            path[path_index++] = cur_node->name[i];
        }
        path[path_index++] = cur_node->name[0]; // Final char

        // Add the preceding '/'
        path[path_index++] = '/';
        cur_node = cur_node->parent;
    }

    unlock(vfs_lock);

    reverse(path);
    return path;
}