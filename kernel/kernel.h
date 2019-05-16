#ifndef KERNEL_H
#define KERNEL_H

void user_input(char *input);
int getstate();
int uinlen;
int prompttype;
int state;
void halt();
void shutdown();
void panic();
void memory();
int stdinpass;

#endif