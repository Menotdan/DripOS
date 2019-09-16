#ifndef STRINGS_H
#define STRINGS_H
#include <stdint.h>
void int_to_ascii(int n, char str[]);
void uint_to_ascii(unsigned int n, char str[]);
void itoa_s(int i,char* buf);
void hex_to_ascii(int n, char str[]);
void reverse(char s[]);
int strlen(char s[]);
void backspace(char s[]);
void append(char s[], char n);
int strcmp(char s1[], char s2[]);
int match(char s1[], char s2[]);
//int first_space(char s1[]);
char *strcpy(char *dest, const char *src);
const char* afterSpace(const char* input);
void kprint_int(int num);
void kprint_uint(unsigned int num);
char to_hex(uint8_t nibble);
void fixer(char s[]);
void sprint_int(int num);
void sprint_uint(unsigned int num);
char *mstrcpy(uint32_t dest, const char *src);

#endif