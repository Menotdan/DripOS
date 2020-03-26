#include "math.h"

int64_t abs(int64_t in) {
    /* If input is less than 0, return the opposite of the input, otherwise return input */
    if (in < 0) return -in;
    return in;
}