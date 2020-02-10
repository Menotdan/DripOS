#ifndef KLIBC_STRING_H
#define KLIBC_STRING_H
#include <stdint.h>

void reverse(char s[]);
void utoa(uint64_t n, char str[]);
void itoa(int64_t n, char str[]);
void htoa(uint64_t in, char str[]);
uint64_t strlen(char str[]);
void memcpy(uint8_t *src, uint8_t *dst, uint64_t count);
void memcpy32(uint32_t *src, uint32_t *dst, uint64_t count);
void memset(uint8_t *dst, uint8_t data, uint64_t count);
void memset32(uint32_t *dst, uint32_t data, uint64_t count);

#endif