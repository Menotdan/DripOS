#ifndef KLIBC_KERN_STATE_H
#define KLIBC_KERN_STATE_H
#include <stdint.h>

typedef struct {
    uint8_t data[255]; // 255 bytes per kernel variable
} kern_state_var;

void setup_kernel_state();

void add_kernel_state(char *name, uint8_t *data, uint8_t byte_count);
int modify_kernel_state(char *name, uint8_t *new_data, uint8_t byte_count);
int load_kernel_state(char *name, uint8_t *buffer, uint8_t byte_count);
void remove_kernel_state(char *name);

#endif