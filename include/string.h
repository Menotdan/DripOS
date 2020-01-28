#ifndef STRINGS_H
#define STRINGS_H
#include <stdint.h>
void int_to_ascii(int n, char str[]);
void int64_to_ascii(int64_t n, char str[]);
void uint_to_ascii(unsigned int n, char str[]);
void uint64_to_ascii(uint64_t n, char str[]);
void itoa_s(int i,char* buf);
void hex_to_ascii(int n, char str[]);
void htoa(uint64_t in, char out[]);
void reverse(char s[]);
int strlen(char s[]);
void backspace(char s[]);
void backspacep(char s[], uint32_t pos);
void append(char s[], char n);
void appendp(char s[], char n, uint32_t position);
int strcmp(const char s1[], char s2[]);
int match(char s1[], char s2[]);
//int first_space(char s1[]);
char *strcpy(char *dest, char *src);
char* afterSpace(char* input);
char to_hex(uint8_t nibble);
void fixer(char s[]);
char *mstrcpy(uint32_t dest, const char *src);
char remove_null(char s[]);
void nntn(char i[], char o[], uint32_t inLen);
void copy_no_null(char i[], char o[]);
void fat_str(char name[], char ext[], char o[]);
int atoi(char *str);

#endif