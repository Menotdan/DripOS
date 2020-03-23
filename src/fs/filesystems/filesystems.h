#ifndef FILESYSTEMS_H
#define FILESYSTEMS_H
#include <stdint.h>

typedef void (*filesystem_gen_node_t) (char *filename);


void register_mountpoint(char *mountpoint, filesystem_gen_node_t node_handler);
uint8_t is_mountpoint(char *path);
void gen_node_mountpoint(char *mountpoint, char *path);
void init_filesystem_handler();

#endif