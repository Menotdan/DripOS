#include "scheduler.h"
#include "klibc/dynarray.h"
#include "klibc/string.h"
#include "drivers/tty/tty.h"

dynarray_new(task_t, tasks);

void schedule(int_reg_t *r) {
    (void) r;
    
    dynarray_init(tasks);

    uint64_t item_count = dynarray_item_count(tasks);
    kprintf("\nItem count %lu", item_count);
    task_t new_task;
    memset((uint8_t *) &new_task, 0, sizeof(task_t));
    dynarray_add(task_t, tasks, &new_task);
    item_count = dynarray_item_count(tasks);
    kprintf("\nItem count new %lu", item_count);
}