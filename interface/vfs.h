#include <stdint.h>
#ifndef VFS_H
#define VFS_H

typedef uint8_t vfs_node_base_t; // this stores the type of the vfs_node

typedef struct vfs_node_list {
    vfs_node_base_t *represents; // The node this list element represents
    struct vfs_node_list *next; // Next in the chain
} vfs_node_list_t;

typedef struct {
   uint8_t type; // the type, needed for casting pointers
   char *name; // Name of the folder
   vfs_node_base_t *children; // A linked list of the children of this folder
} vfs_folder_t;

typedef struct {
    uint8_t type; // the type, needed for casting pointers
    char *name; // Name of the file
} vfs_file_t;

typedef struct {
    vfs_folder_t *root_dir;
} vfs_root_t; // Root node of a vfs

#endif