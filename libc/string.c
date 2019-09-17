#include "string.h"
#include "../cpu/types.h"
#include "../drivers/screen.h"
#include <stdint.h>
#include <serial.h>
#include <libc.h>

/**
 * K&R implementation
 */
void int_to_ascii(int n, char str[]) {
    int i, sign;
    if ((sign = n) < 0) n = -n; // if number is less than 0, invert it
    i = 0;
    do {
        str[i++] = n % 10 + '0';
    } while ((n /= 10) > 0);

    if (sign < 0) str[i++] = '-';
    str[i] = '\0';

    reverse(str);
}

void uint_to_ascii(unsigned int n, char str[]) {
    int i;
    i = 0;
    do {
        str[i++] = n % 10 + '0';
    } while ((n /= 10) > 0);

    str[i] = '\0';

    reverse(str);
}

void itoa_s(int i,char* buf) {
   if (i < 0) {
      *buf++ = '-';
      i *= -1;
   }
   int_to_ascii(i,buf);
}
int atoi(char *str) 
{ 
    int result = 0; // Where the result will be
   
    //  For each position in the char, convert to int and store in the result 
    for (int i = 0; str[i] != '\0'; ++i) 
        result = result*10 + str[i] - '0'; 
   
    // Return the result
    return result; 
} 


void hex_to_ascii(int n, char str[]) {
    append(str, '0');
    append(str, 'x');
    char zeros = 0;

    int32_t tmp;
    int i;
    for (i = 28; i > 0; i -= 4) {
        tmp = (n >> i) & 0xF;
        if (tmp == 0 && zeros == 0) continue;
        zeros = 1;
        if (tmp > 0xA) append(str, tmp - 0xA + 'a');
        else append(str, tmp + '0');
    }

    tmp = n & 0xF;
    if (tmp >= 0xA) append(str, tmp - 0xA + 'a');
    else append(str, tmp + '0');
}

/* K&R */
void reverse(char s[]) {
    int c, i, j;
    for (i = 0, j = strlen(s)-1; i < j; i++, j--) {
        c = s[i];
        s[i] = s[j];
        s[j] = c;
    }
}

void kprint_int(int num) {
    char toprint[33];
    int_to_ascii(num, toprint);
    kprint(toprint);
}

void sprint_int(int num) {
    char toprint[33];
    int_to_ascii(num, toprint);
    sprint(toprint);
}

void kprint_uint(unsigned int num) {
    char toprint[33];
    uint_to_ascii(num, toprint);
    kprint(toprint);
}

void sprint_uint(unsigned int num) {
    char toprint[33];
    uint_to_ascii(num, toprint);
    sprint(toprint);
}

/* K&R */
int strlen(char s[]) {
    int i = 0;
    while (s[i] != '\0') ++i;
    return i;
}

void append(char s[], char n) {
    int len = strlen(s);
    s[len] = n;
    s[len+1] = '\0';
}

void appendp(char s[], char n, uint32_t position) {
    int len = strlen(s);
    uint32_t i;
    char z = n;
    char tempf;
    for (i = position; i < len + 1; i++)
    {
        tempf = s[i];
        s[i] = z;
        z = tempf;
    }
    s[len+1] = '\0';
}

void backspace(char s[]) {
    int len = strlen(s);
    s[len-1] = '\0';
}

void backspacep(char s[], uint32_t pos) {
    int len = strlen(s);
    char prev = "\0";
    char tempE;
    for (uint32_t x = len; x > pos-2; x--) {
        tempE = s[x];
        s[x] = prev;
        prev = tempE;
    }
    s[len-1] = '\0';
}

/* Fix for a random bug that I don't know how to fix but i will try */
void fixer(char s[]) {
    int len = strlen(s);
    for (int i = 0; i < len; i++) {
        if (s[i] != '\0') {
            s[i] = s[i];
        }
    }
}

/* K&R 
 * Returns <0 if s1<s2, 0 if s1==s2, >0 if s1>s2 */
int strcmp(char s1[], char s2[]) {
    int i;
    for (i = 0; s1[i] == s2[i]; i++) {
        if (s1[i] == '\0') return 0;
    }
    return s1[i] - s2[i];
}

int match(char s1[], char s2[]) { //how many characters match, before the first unmatching character
    int ret;
    for (int i = 0; s1[i] == s2[i]; i++) {
        //char hello[100];
        //int_to_ascii(i, hello);
        //kprint(hello);
        if (s1[i] == "\0" || s2[i] == "\0") return i - 1;
        ret = i;
    }
    if (strcmp(s1, s2) == 0) {
        return -2;
    }
    return ret;
}

char *strcpy(char *dest, const char *src)
{
  unsigned i;
  for (i=0; src[i] != '\0'; ++i)
    dest[i] = src[i];

  //Ensure trailing null byte is copied
  dest[i]= '\0';

  return dest;
}

char *mstrcpy(uint32_t dest, const char *src)
{
  unsigned i;
  char *current;
  for (i=0; src[i] != '\0'; ++i) {
    current = (char *)get_pointer(dest + i);
    *current = src[i];
  }

  //Ensure trailing null byte is copied
  current = (char *)get_pointer(dest + i);
  *current = '\0';

  return (char *)get_pointer(dest);
}

const char* afterSpace(const char* input) {
   const char* starting = input;
   while (*starting != ' ') {
     starting++;
   }
   // first one _after_
   starting++;
   return starting;
 } 

char to_hex(uint8_t nibble)
{
   static const char hex[] = "0123456789ABCDEF";
   return hex[nibble];
}