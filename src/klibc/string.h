#ifndef KLIBC_STRING_H
#define KLIBC_STRING_H
#include <stdint.h>

void reverse(char s[]);
void utoa(uint64_t n, char str[]);
void itoa(int64_t n, char str[]);
void htoa(uint64_t in, char str[]);
uint64_t strlen(char str[]);

#endif