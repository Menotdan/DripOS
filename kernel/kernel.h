#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>

void user_input(char input[]);
int getstate();
uint32_t uinlen;
uint32_t position;
int prompttype;
int state;
void halt();
void shutdown();
void panic();
void memory();
int stdinpass;
int loaded;
char key_buffer[2000];
char key_buffer_up[2000];
char key_buffer_down[2000];

#endif