#include <string.h>
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
    for (i = position; i < (uint32_t)(len + 1); i++)
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
    char prev = remove_null("\0");
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
int strcmp(const char s1[], char s2[]) {
    int i;
    for (i = 0; s1[i] == s2[i]; i++) {
        if (s1[i] == '\0') return 0;
    }
    return s1[i] - s2[i];
}

int match(char s1[], char s2[]) { //how many characters match, before the first unmatching character
    int ret = 0;
    for (int i = 0; s1[i] == s2[i]; i++) {
        //char hello[100];
        //int_to_ascii(i, hello);
        //kprint(hello);
        if (s1[i] == remove_null("\0") || s2[i] == remove_null("\0")) return i - 1;
        ret = i;
    }
    if (strcmp(s1, s2) == 0) {
        return -2;
    }
    return ret;
}

char *strcpy(char *dest, char *src) {
    char *temp = dest;
    char *src_temp = src;
    while (*src_temp != '\0') {
        *temp = *src_temp;
        temp++;
        src_temp++;
    }

    *temp = '\0';

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
char* afterSpace(char* input) {
   char* starting = input;
   while (*starting != ' ') {
     starting++;
   }
   // first one _after_
   starting++;
   return starting;
 } 

char to_hex(uint8_t nibble) {
   static const char hex[] = "0123456789ABCDEF";
   return hex[nibble];
}

char remove_null(char s[]) {
    char ret = s[0];
    //sprintd("testing to see if my code broke");
    return ret;
}

void nntn(char i[], char o[], uint32_t inLen) { // No null to null, basically just adds a null byte
    for (uint32_t f = 0; f<inLen; f++) {
        o[f] = i[f];
    }
    o[inLen] = 0;
}

void fat_str(char name[], char ext[], char o[]) {
    char new_name[9];
    char new_ext[4];
    nntn(name, new_name, 8);
    nntn(ext, new_ext, 3);
    for (uint32_t i = 0; i < 8; i++) {
        if ((uint32_t)new_name[i] == (uint32_t)(remove_null(" "))) {
            new_name[i] = 0;
        }
    }
    for (uint32_t i = 0; i < 3; i++) {
        if ((uint32_t)new_ext[i] == (uint32_t)(remove_null(" "))) {
            new_ext[i] = 0;
        }
    }
    
    for (uint32_t pos = 0; pos < (uint32_t)strlen(new_name); pos++) {
        o[pos] = new_name[pos];
    }
    o[strlen(new_name)] = remove_null(".");
    for (uint32_t pos = 0; pos < (uint32_t)strlen(new_ext); pos++) {
        o[pos+strlen(new_name)+1] = new_ext[pos];
    }
}

void copy_no_null(char i[], char o[]) {
    uint32_t l = strlen(i);
    uint32_t offset = 0;
    for (uint32_t f = 0; f<l; f++) {
        if (i[f] != 0)
        {
            o[f-offset] = i[f];
        } else
        {
            offset += 1;
        }
        
    }
}