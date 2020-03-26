#ifndef FILESYSTEMS_H
#define FILESYSTEMS_H
#include <stdint.h>

typedef void (*filesystem_gen_node_t) (void *filesystem_descriptor, char *filename);

typedef struct {
    filesystem_gen_node_t node_handler;
    void *filesystem_descriptor;
} filesystem_mountpoint_info_t;

void register_mountpoint(char *mountpoint, filesystem_gen_node_t node_handler, void *filesystem_descriptor);
void register_unid(uint64_t unid, void *filesystem_dat);
void *get_unid_fs_data(uint64_t unid);
uint8_t is_mountpoint(char *path);
void gen_node_mountpoint(char *mountpoint, char *path);
void init_filesystem_handler();

#endif