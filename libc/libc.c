#include <libc.h>

unsigned int abs(int num) {
    int temp;
    unsigned int ret;
    if ((temp = num) < 0) temp = -temp;
    ret = temp;
    return temp;
}