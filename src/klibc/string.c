#include "string.h"
#include "math.h"
#include "drivers/serial.h"

uint64_t strlen(char str[]) {
    uint64_t len = 0;
    while (str[len++] != '\0');
    return len - 1;
}

void strcpy(char *src, char *dst) {
    uint64_t len = strlen(src);
    for (uint64_t i = 0; i < len; i++)
        *dst++ = *src++;
}

int strcmp(char *s1, char *s2) {
    while (*s1 != '\0' && *s2 != '\0') {
        if (*s1 != *s2) { return 1; }
        s1++;
        s2++;
    }
    return 0;
}

void reverse(char str[]) {
    uint64_t start_index = 0;
    uint64_t end_index = 0;
    if (strlen(str) > 0) {
        end_index = strlen(str) - 1;
    }
    char temp_buffer = 0;
    // Two indexes, one starts at 0, the other starts at strlen - 1
    // On each iteration, store the char at start_index in temp_buffer,
    // Store the char at end_index in the space at start_index,
    // And then put the char in temp buffer in the space at end_index
    // And finally decrement end_index and increment start_index
    while (start_index < end_index) {
        temp_buffer = str[start_index];
        str[start_index] = str[end_index];
        str[end_index] = temp_buffer;
        start_index++;
        end_index--;
    }
}

void itoa(int64_t n, char str[]) {
    uint64_t i;
    i = 0;
    int64_t n_inv = abs(n); // inverted n, if it was negative
    uint64_t abs_n = (uint64_t) n_inv;
    if (abs_n == 0) {
        str[i++] = '0';
    }
    while (abs_n > 0) {
        str[i++] = (abs_n % 10) + '0'; // Calculate the current lowest value digit and convert it
        abs_n /= 10;
    }

    if (n_inv != n) {
        str[i++] = '-';
    }

    str[i] = '\0';

    reverse(str);
}

void utoa(uint64_t n, char str[]) {
    uint64_t i;
    i = 0;
    if (n == 0) {
        str[i++] = '0';
    }
    while (n > 0) {
        str[i++] = (n % 10) + '0'; // Calculate the current lowest value digit and convert it
        n /= 10;
    }

    str[i] = '\0';

    reverse(str);
}

void htoa(uint64_t in, char str[]) {
    uint32_t pos = 0;
    uint8_t tmp;

    str[pos++] = '0';
    str[pos++] = 'x';

    for (uint16_t i = 60; i > 0; i -= 4) {
        tmp = (uint8_t)((in >> i) & 0xf);
        if (tmp >= 0xa) {
            str[pos++] = (tmp - 0xa) + 'A';
        } else {
            str[pos++] = tmp + '0';
        }
    }

    tmp = (uint8_t)(in & 0xf);
    if (tmp >= 0xa) {
        str[pos++] = (tmp - 0xa) + 'A';
    } else {
        str[pos++] = tmp + '0';
    }
    str[pos] = '\0'; // nullify 
}

void memcpy(uint8_t *src, uint8_t *dst, uint64_t count) {
    for (uint64_t i = 0; i<count; i++)
        *dst++ = *src++;
}

void memcpy32(uint32_t *src, uint32_t *dst, uint64_t count) {
    for (uint64_t i = 0; i<count; i++)
        *dst++ = *src++;
}

void memset(uint8_t *dst, uint8_t data, uint64_t count) {
    for (uint64_t i = 0; i<count; i++)
        *dst++ = data;
}

void memset32(uint32_t *dst, uint32_t data, uint64_t count) {
    for (uint64_t i = 0; i<count; i++)
        *dst++ = data;
}

/* TODO: REMOVE THESE LMAO */
void dgbmemcpy32(uint32_t *src, uint32_t *dst, uint64_t count, uint64_t low_limit, uint64_t high_limit) {
    for (uint64_t i = 0; i<count; i++) {
        if ((uint64_t) dst > high_limit || (uint64_t) dst < low_limit 
            || (uint64_t) src > high_limit || (uint64_t) src < low_limit) {
            sprintf("\nBroke limit with %lx and %lx", (uint64_t) dst, (uint64_t) src);
            while (1) {}
            
        }
        *dst++ = *src++;
    }
}

void dbgmemset32(uint32_t *dst, uint32_t data, uint64_t count, uint64_t low_limit, uint64_t high_limit) {
    sprintf("\nMemset 32: %lx, %u, %lu", dst, (uint32_t) data, count * 4);
    for (uint64_t i = 0; i<count; i++) {
        if ((uint64_t) dst > high_limit || (uint64_t) dst < low_limit) {
            sprintf("\nBroke limit with %lx", (uint64_t) dst);
            while (1) {}
        }
        *dst++ = data;
    }
}