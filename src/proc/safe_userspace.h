#ifndef SAFE_USERSPACE_H
#define SAFE_USERSPACE_H
#include <stdint.h>

int64_t strcpy_from_userspace(char *src, char *dst);

#endif