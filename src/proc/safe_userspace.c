#include "safe_userspace.h"
#include "mm/vmm.h"

int64_t strcpy_from_userspace(char *src, char *dst) {
    for (uint64_t i = 0; i<4096; i++) {
        if (!range_mapped(src, 1)) {
            return -1;
        }
        if (*src == '\0') {
            *dst = '\0';
            return 0;        
        }
        *dst = *src;
        dst++;
        src++;
    }
    *dst = '\0';
    return 0;
}