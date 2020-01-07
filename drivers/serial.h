#ifndef SERIAL_H
#define SERIAL_H

void init_serial();
void write_serial(char a);
void sprint(char *message);
void sprintd(char *message);
void sprint_uint(unsigned int num);
void sprint_int(int num);

#endif