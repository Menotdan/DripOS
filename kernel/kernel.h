#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>

#define MEMORY_MAPPED 0x2800000

void user_input(char input[]);
int getstate();
void Log(char *message, int type);
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
uint8_t *vidmem;
uint16_t width;
uint16_t height;
uint32_t bbp; // Bytes, not bits
uint32_t extra_bits;
uint32_t bpl; // Bytes per line
uint8_t red_byte;
uint8_t blue_byte;
uint8_t green_byte;
uint32_t char_w;
uint32_t char_h;

#endif