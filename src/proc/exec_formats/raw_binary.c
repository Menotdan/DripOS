#include "raw_binary.h"
#include "klibc/stdlib.h"
#include "proc/scheduler.h"
#include "mm/vmm.h"
#include "fs/fd.h"
#include "drivers/serial.h"

static void new_binary_process(char *process_name, void *exec_ptr, void *code, uint64_t program_size) {
    void *rsp = kcalloc(TASK_STACK_SIZE);
    void *rsp_phys = GET_LOWER_HALF(void *, rsp);

    task_t *new_thread = create_thread(process_name, exec_ptr, USER_STACK, 3);

    void *cr3 = vmm_fork_higher_half((void *) vmm_get_pml4t());
    int64_t pid = new_process(process_name, cr3);

    void *code_phys = GET_LOWER_HALF(void *, code);

    /* Map memory and mark it as used in the process's address space */
    map_user_memory(pid, code_phys, exec_ptr, program_size, VMM_WRITE);
    map_user_memory(pid, rsp_phys, (void *) USER_STACK_START, TASK_STACK_SIZE, VMM_WRITE);
    mark_process_mem_used(pid, (void *) USER_STACK_START, TASK_STACK_SIZE);
    mark_process_mem_used(pid, exec_ptr, program_size);
    
    add_new_child_thread(new_thread, pid);
}

void launch_binary(char *path) {
    int fd = fd_open(path, 0);
    if (fd >= 0) {
        void *data_buf = kcalloc(0x1000);
        int read_bytes = fd_read(fd, data_buf, 0);
        fd_close(fd);

        sprintf("\nLoading binary of size %d for execution", read_bytes);
        new_binary_process(path, (void *) 0, data_buf, read_bytes);
    } else {
        sprintf("\nGot error loading binary: %d", get_thread_locals()->errno);
    }
}