#include <stdint.h>
#include <stddef.h>

#include "mm/pmm.h"

#include "fs/vfs/vfs.h"
#include "fs/devfs/devfs.h"
#include "fs/fd.h"

#include "sys/apic.h"
#include "sys/int/isr.h"

#include "klibc/stdlib.h"

#include "drivers/pit.h"
#include "drivers/serial.h"
#include "drivers/vesa.h"
#include "drivers/tty/tty.h"
#include "drivers/pci.h"
#include "drivers/ps2.h"
#include "drivers/rtc.h"

#include "dripdbg/debug.h"

#include "stivale.h"

/* Testing includes */
#include "proc/scheduler.h"
#include "proc/exec_formats/raw_binary.h"
#include "proc/sleep_queue.h"
#include "proc/syscalls.h"
#include "io/msr.h"

#include "klibc/string.h"
#include "klibc/math.h"
#include "sys/smp.h"

#include "fs/filesystems/echfs.h"
#include "proc/exec_formats/elf.h"
#include "proc/event.h"
#include "proc/urm.h"
#include "proc/ipc.h"

#define TODO_LIST_SIZE 1
char *todo_list[TODO_LIST_SIZE] = {"git gud"};

void kernel_process(int argc, char **argv, auxv_t *auxv) {
    fd_write(0, "Hello from stdout!\n", 19);
    kprintf("argc: %d, argv: %lx\n", argc, argv);
    kprintf("auxv = %lx\n", auxv);
    if (argv) {
        for (int i = 0; i < argc; i++) {
            kprintf("argv: %s\n", argv[i]);
        }
    }

    pci_init(); // Setup PCI devices and their drivers

    kprintf("[DripOS Kernel] Bultin todo list:\n");
    for (uint64_t i = 0; i < TODO_LIST_SIZE; i++) {
        kprintf("  %s\n", todo_list[i]);
    }

    echfs_test("/dev/satadeva");
    kprintf("Memory used: %lu bytes\n", pmm_get_used_mem());
    mouse_setup();

    setup_ipc_servers();

    kprintf("Loading DripOS init!\n");
    int64_t init_pid = load_elf("/bin/init");
    sprintf("Elf PID: %ld\n", init_pid);
    
    if (init_pid == -1) {
        panic("Init failed to load!");
    }

    sprintf("done kernel work\n");

#ifdef DBGPROTO
    setup_drip_dgb();
#endif

    kill_task(get_cpu_locals()->current_thread->tid); // suicide
    sprintf("WHY DID THIS RETURN!?\n");
}

void kernel_task() {
    sprintf("[DripOS] Kernel thread: Scheduler enabled.\n");

    kprintf("[DripOS] Loading VFS\n");
    vfs_init(); // Setup VFS
    devfs_init();
    kprintf("[DripOS] Loaded VFS\n");
    vfs_ops_t ops = dummy_ops;
    ops.open = devfs_open;
    ops.close = devfs_close;
    ops.write = tty_dev_write;
    ops.read = tty_dev_read;
    register_device("tty1", ops, (void *) 0);

    kprintf("Setting up PID 0...\n");
    new_kernel_process("Kernel process", kernel_process);
    kprintf("Starting URM...\n");
    thread_t *urm = create_thread("URM", urm_thread, (uint64_t) kcalloc(TASK_STACK_SIZE) + TASK_STACK_SIZE, 0);
    add_new_child_thread(urm, 0);

    kill_task(get_cpu_locals()->current_thread->tid); // suicide
    sprintf("WHY DID THIS RETURN 2!?\n");
}

// Kernel main function, execution starts here :D
void kmain(stivale_info_t *bootloader_info) {
    init_serial(COM1);
    sprintf("\e[34mHello from DripOS!\e[0m\n");

    char *kernel_options = (char *) ((uint64_t) bootloader_info->cmdline + 0xFFFF800000000000);
    sprintf("Kernel Options: %s\n", kernel_options);

    if (bootloader_info) {
        sprintf("[DripOS] Setting up memory bitmaps.\n");
        pmm_memory_setup(bootloader_info);
    }

    sprintf("[DripOS] Initializing TTY\n");
    init_vesa(bootloader_info);
    tty_init(&base_tty, 8, 8);

    if (strcmp(kernel_options, "nocrash") == 1) {
        panic("Kernel option 'nocrash' not specified!");
    }

    acpi_init(bootloader_info);
    sprintf("[DripOS] Configuring LAPICs and IOAPIC routing.\n");
    configure_apic();

    new_cpu_locals(); // Setup CPU locals for our CPU
    get_cpu_locals()->apic_id = get_lapic_id();
    get_cpu_locals()->cpu_index = 0;
    hashmap_set_elem(cpu_locals_list, 0, get_cpu_locals());

    load_tss();
    set_panic_stack((uint64_t) kmalloc(0x1000) + 0x1000);
    set_kernel_stack((uint64_t) kmalloc(0x1000) + 0x1000);

    sprintf("[DripOS] Registering interrupts and setting interrupt flag.\n");
    configure_idt();
    sprintf("[DripOS] Setting timer speed to 1000 hz.\n");
    set_pit_freq();
    sprintf("[DripOS] Timers set.\n");

    sprintf("[DripOS] Set kernel stacks.\n");
    scheduler_init_bsp();

    thread_t *kernel_thread = create_thread("Kernel setup worker", kernel_task, 
        (uint64_t) kmalloc(TASK_STACK_SIZE) + TASK_STACK_SIZE, 0);
    start_thread(kernel_thread);

    sprintf("[DripOS] Launched kernel thread, scheduler disabled...\n");

    sprintf("[DripOS] Launching all SMP cores...\n");
    launch_cpus();
    sprintf("[DripOS] Finished loading SMP cores.\n");

    init_syscalls();

    tty_clear(&base_tty);

    sprintf("[DripOS] Loading scheduler...\n");
    scheduler_enabled = 1;

    while (1) {
        asm volatile("hlt");
    }
}