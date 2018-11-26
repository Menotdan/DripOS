#ifndef STRINGS_H
#define STRINGS_H

void int_to_ascii(int n, char str[]);
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

#endif