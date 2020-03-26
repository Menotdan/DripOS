#ifndef KLIBC_MATH_H
#define KLIBC_MATH_H
#include <stdint.h>

#define ROUND_UP(N, S) ((((N) + (S) - 1) / (S)) * (S))

int64_t abs(int64_t in);

#endif