#include <stdint.h>
#include <stddef.h>

#include "mm/pmm.h"

#include "fs/vfs/vfs.h"
#include "fs/devfs/devfs.h"
#include "fs/fd.h"

#include "sys/apic.h"
#include "sys/int/isr.h"

#include "klibc/stdlib.h"
#include "klibc/logger.h"

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
#include "klibc/debug.h"
#include "sys/smp.h"

#include "fs/filesystems/echfs.h"
#include "proc/exec_formats/elf.h"
#include "proc/event.h"
#include "proc/urm.h"
#include "proc/ipc.h"

#define TODO_LIST_SIZE 1
char *todo_list[TODO_LIST_SIZE] = {"git gud"};

void kernel_process() {
    log("Setting up PCI.");
    pci_init(); // Setup PCI devices and their drivers
    log("Set up PCI and loaded all available drivers.");

    kprintf("[DripOS Kernel] Bultin todo list:\n");
    for (uint64_t i = 0; i < TODO_LIST_SIZE; i++) {
        kprintf("  %s\n", todo_list[i]);
    }

    echfs_test("/dev/satadeva");
    kprintf("Memory used: %lu bytes\n", pmm_get_used_mem());
    mouse_setup();

    log("Setting up kernel's IPC servers from drivers and such.");
    setup_ipc_servers();

    kprintf("Loading DripOS init!\n");
    log("Creating an instance of init.");
    int64_t init_pid = load_elf("/bin/init");
    
    if (init_pid == -1) {
        panic("Init failed to load!");
    }
    log("Init loaded, exiting kernel setup thread, kernel process remains.");
#ifdef DBGPROTO
    setup_drip_dgb();
#endif

    kill_task(get_cpu_locals()->current_thread->tid); // suicide
}

void kernel_task() {
    vfs_init(); // Setup VFS
    devfs_init();
    log("VFS loaded into memory and ready.");

    vfs_ops_t ops = dummy_ops;
    ops.open = devfs_open;
    ops.close = devfs_close;
    ops.write = tty_dev_write;
    ops.read = tty_dev_read;
    register_device("tty1", ops, (void *) 0);

    log("Starting kernel process and userspace request monitor thread under kernel process.");
    new_kernel_process("Kernel process", kernel_process);
    thread_t *urm = create_thread("URM", urm_thread, (uint64_t) kcalloc(TASK_STACK_SIZE) + TASK_STACK_SIZE, 0);
    add_new_child_thread(urm, 0);
    log("URM started and kernel process started, exiting kernel_task.");

    kill_task(get_cpu_locals()->current_thread->tid); // suicide
}

// Kernel main function, execution starts here :D
void kmain(stivale_info_t *bootloader_info) {
    init_serial(COM1);
    log("DripOS serial started. Hello from the kernel.");
    sprintf("\e[34mHello from DripOS!\e[0m\n");

    char *kernel_options = (char *) ((uint64_t) bootloader_info->cmdline + 0xFFFF800000000000);
    sprintf("Kernel Options: %s\n", kernel_options);

    if (bootloader_info) {
        pmm_memory_setup(bootloader_info);
        log("PMM enabled, virtual memory set up.");
    }

    init_vesa(bootloader_info);
    tty_init(&base_tty, 8, 8);

    if (strcmp(kernel_options, "nocrash") == 1) {
        panic("Kernel option 'nocrash' not specified!");
    }

    acpi_init(bootloader_info);
    log("ACPI initalized.");

    configure_apic();
    log("APIC configured and routed.");

    new_cpu_locals(); // Setup CPU locals for our CPU
    get_cpu_locals()->apic_id = get_lapic_id();
    get_cpu_locals()->cpu_index = 0;
    hashmap_set_elem(cpu_locals_list, 0, get_cpu_locals());

    load_tss();
    set_panic_stack((uint64_t) kmalloc(0x1000) + 0x1000);
    set_kernel_stack((uint64_t) kmalloc(0x1000) + 0x1000);

    configure_idt();
    log("Interrupts enabled.");

    set_pit_freq();
    log("PIT enabled at 1000hz.");

    scheduler_init_bsp();

    thread_t *kernel_thread = create_thread("Kernel setup worker", kernel_task, 
        (uint64_t) kmalloc(TASK_STACK_SIZE) + TASK_STACK_SIZE, 0);
    start_thread(kernel_thread);

    log("Kernel thread in memory, starting CPUs and enabling scheduler.");

    launch_cpus();
    log("SMP setup and all cores running.");

    init_syscalls();
    log("Setup syscall table.");

    tty_clear(&base_tty);

    sprintf("[DripOS] Loading scheduler...\n");
    log("Scheduler starting, kmain log end.");
    scheduler_enabled = 1;

    while (1) {
        asm volatile("hlt");
    }
}