#include "kern_state.h"
#include "klibc/strhashmap.h"
#include "klibc/string.h"
#include "klibc/stdlib.h"
#include "drivers/serial.h"

strhashmap_t *kernel_variables;
int kernel_state_ready = 0;

void add_kernel_state(char *name, uint8_t *data, uint8_t byte_count) {
    if (kernel_state_ready) {
        kern_state_var *new_var = kcalloc(sizeof(kern_state_var));
        memcpy(data, new_var->data, byte_count);

        strhashmap_set_elem(kernel_variables, name, new_var);
    }
}

int modify_kernel_state(char *name, uint8_t *new_data, uint8_t byte_count) {
    if (kernel_state_ready) {
        kern_state_var *old = strhashmap_get_elem(kernel_variables, name);
        if (!old) {
            return 0;
        }

        memcpy(new_data, old->data, byte_count);
        return 1;
    } else {
        return 0;
    }
}

int load_kernel_state(char *name, uint8_t *buffer, uint8_t byte_count) {
    if (kernel_state_ready) {
        asm("int $3");
        kern_state_var *loaded = strhashmap_get_elem(kernel_variables, name);
        if (loaded) {
            memcpy(loaded->data, buffer, byte_count);
            return 1;
        }

        return 0;
    } else {
        return 0;
    }
}

void remove_kernel_state(char *name) {
    if (kernel_state_ready) {
        kern_state_var *remove = strhashmap_get_elem(kernel_variables, name);
        strhashmap_remove_elem(kernel_variables, name);
        if (remove) {
            kfree(remove);
        }
    }
}

void setup_kernel_state() {
    kernel_variables = init_strhashmap();
    sprintf("\nKernel variables ptr: %lx", kernel_variables);
    kernel_state_ready = 1;
}