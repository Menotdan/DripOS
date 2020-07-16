#ifndef KLIBC_STRING_H
#define KLIBC_STRING_H
#include <stdint.h>

typedef struct linked_strings {
    char *string;
    struct linked_strings *next;
    struct linked_strings *prev;
} linked_strings_t;

/* String manipulation functions */
void reverse(char s[]);
void strcpy(char *src, char *dst);
int strcmp(char *s1, char *s2);
int strncmp(char *s1, char *s2, int len);
void strcat(char *dst, char *src);
uint64_t strlen(char str[]);

/* Path string operations */
void path_join(char *dst, char *src);
char *get_path_elem(char *path, char *output);
char *ptr_to_end_path_elem(char *path);
void path_remove_elem(char *path);

/* Integer functions */
void utoa(uint64_t n, char str[]);
uint64_t atou(char str[]);
void itoa(int64_t n, char str[]);
void htoa(uint64_t in, char str[]);
void endian_reverse(uint8_t *buffer, uint64_t word_count);

/* Memory functions */
void memcpy(uint8_t *src, uint8_t *dst, uint64_t count);
void memcpy32(uint32_t *src, uint32_t *dst, uint64_t count);
void memcpy64(uint64_t *src, uint64_t *dst, uint64_t count);
void memset(uint8_t *dst, uint8_t data, uint64_t count);
void memset32(uint32_t *dst, uint32_t data, uint64_t count);

#endif