#include "safe_userspace.h"
#include "mm/vmm.h"
#include "klibc/stdlib.h"
#include "proc/scheduler.h"
#include "sys/smp.h"

char *check_and_copy_string(char *userspace_string) {
    uint64_t string_length = 0;
    int found_string_null = 0;
    for (uint64_t i = 0; i < 4096; i++) {
        if (!range_mapped((void *) ((uint64_t) userspace_string + i), sizeof(char)) || ((uint64_t) userspace_string + 1 + i > 0x7fffffffffff && get_cur_thread()->ring == 3 && (!get_cpu_locals()->ignore_ring))) {
            sprintf("mapping error\n");
            if (!range_mapped((void *) ((uint64_t) userspace_string + i), sizeof(char))) {
                sprintf("range_mapped check failed (addr: %lx)\n", ((uint64_t) userspace_string + i));
            }
            if ((((uint64_t) userspace_string + 1 + i) > 0x7fffffffffff && get_cur_thread()->ring == 3 && (!get_cpu_locals()->ignore_ring))) {
                sprintf("userspace check failed (current ring: %u) address: %lx\n", get_cur_thread()->ring, (uint64_t) userspace_string + 1 + i);
            }
            return (void *) 0;
        }
        string_length++;
        if (!userspace_string[i]) {
            found_string_null = 1;
            break;
        }
    }

    if (!found_string_null) {
        sprintf("failed to find null in string\n");
        return (void *) 0;
    }

    char *ret = kcalloc(string_length);
    for (uint64_t i = 0; i < string_length; i++) {
        ret[i] = userspace_string[i];
    }

    return ret;
}