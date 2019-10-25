#include <libc.h>

unsigned int abs(int num) {
    if ((num) < 0) return (uint32_t)-num;
    return (uint32_t)num;
}