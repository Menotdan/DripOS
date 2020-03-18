#ifndef KLIBC_STRING_H
#define KLIBC_STRING_H
#include <stdint.h>

typedef struct linked_strings {
    char *string;
    struct linked_strings *next;
    struct linked_strings *prev;
} linked_strings_t;

void reverse(char s[]);
void strcpy(char *src, char *dst);
int strcmp(char *s1, char *s2);
void strcat(char *dst, char *src);
void path_join(char *dst, char *src);
char *get_path_elem(char *path, char *output);
void utoa(uint64_t n, char str[]);
void itoa(int64_t n, char str[]);
void htoa(uint64_t in, char str[]);
uint64_t strlen(char str[]);
void memcpy(uint8_t *src, uint8_t *dst, uint64_t count);
void memcpy32(uint32_t *src, uint32_t *dst, uint64_t count);
void memset(uint8_t *dst, uint8_t data, uint64_t count);
void memset32(uint32_t *dst, uint32_t data, uint64_t count);

/* remove these debug functions */
void dgbmemcpy32(uint32_t *src, uint32_t *dst, uint64_t count, uint64_t low_limit, uint64_t high_limit);
void dbgmemset32(uint32_t *dst, uint32_t data, uint64_t count, uint64_t low_limit, uint64_t high_limit);

#endif