#include "cpu_index.h"
#include "smp.h"

int get_cpu_index() {
    return get_cpu_locals()->cpu_index;
}