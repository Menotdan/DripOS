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

#include "dripdbg/debug.h"

#include "stivale.h"

/* Testing includes */
#include "proc/scheduler.h"
#include "proc/exec_formats/raw_binary.h"
#include "proc/sleep_queue.h"
#include "io/msr.h"

#include "klibc/string.h"
#include "klibc/math.h"
#include "sys/smp.h"

#include "fs/filesystems/echfs.h"

#define TODO_LIST_SIZE 7
char *todo_list[TODO_LIST_SIZE] = {"Better syscall error handling", "Filesystem driver", "ELF Loading", "userspace libc", "minor: Sync TLB across CPUs", "minor: Add MMIO PCI", "minor: Retry AHCI commands"};


// Testing sleep
uint64_t delay = 5000;
uint64_t y = 300;
uint8_t random_num = 128;

void random_thread() {
    while (1) random_num = random(255);
}

void video_thread() {
    while (1) {
        for (uint64_t y = 0; y < vesa_display_info.height; y++) {
            for (uint64_t x = 0; x < vesa_display_info.width; x++) {
                color_t color = {random_num, random_num, random_num};
                put_pixel(x, y, color);
            }
            sleep_ms(16);
            flip_buffers();
        }
    }
}

extern void sanity_thread_start();

void kernel_task() {
    sprintf("\n[DripOS] Kernel thread: Scheduler enabled.");

    kprintf("\n[DripOS] Loading VFS");
    vfs_init(); // Setup VFS
    devfs_init();
    kprintf("\n[DripOS] Loaded VFS");
    vfs_ops_t ops = dummy_ops;
    ops.open = devfs_open;
    ops.close = devfs_close;
    ops.write = tty_dev_write;
    ops.read = tty_dev_read;
    register_device("tty1", ops, (void *) 0);

    pci_init(); // Setup PCI devices and their drivers

    kprintf("\n[DripOS Kernel] Bultin todo list:");
    for (uint64_t i = 0; i < TODO_LIST_SIZE; i++) {
        kprintf("\n  %s", todo_list[i]);
    }

    echfs_test("/dev/satadeva");

    new_kernel_process("random", random_thread);
    new_kernel_process("random", random_thread);
    new_kernel_process("random", random_thread);
    new_kernel_process("random", random_thread);
    new_kernel_process("random", random_thread);
    new_kernel_process("random", random_thread);
    new_kernel_process("random", random_thread);
    new_kernel_process("Video", video_thread);

    kprintf("\nMemory used: %lu bytes", pmm_get_used_mem());
    mouse_setup();

    kprintf("\n[DripOS] Loading binary from disk.\n");
    launch_binary("/echfs_mount/programs/program_1.bin");

    // while (1) {
    //     sleep_ms(100);
    //     sprintf("\nuwu");
    // }

    sprintf("\ndone kernel work");

#ifdef DBGPROTO
    setup_drip_dgb();
#endif
    while (1) { asm volatile("hlt"); }
}

// Kernel main function, execution starts here :D
void kmain(stivale_info_t *bootloader_info) {
    init_serial(COM1);

    if (bootloader_info) {
        sprintf("[DripOS] Setting up memory bitmaps.");
        pmm_memory_setup(bootloader_info);
    }

    sprintf("\n[DripOS] Initializing TTY");
    init_vesa(bootloader_info);
    tty_init(&base_tty, 8, 8);

    acpi_init(bootloader_info);
    sprintf("\n[DripOS] Configuring LAPICs and IOAPIC routing.");
    configure_apic();

    new_cpu_locals(); // Setup CPU locals for our CPU
    load_tss();
    set_panic_stack((uint64_t) kmalloc(0x1000) + 0x1000);
    set_kernel_stack((uint64_t) kmalloc(0x1000) + 0x1000);


    sprintf("\n[DripOS] Set kernel stacks.");
    scheduler_init_bsp();

    sprintf("\n[DripOS] Registering interrupts and setting interrupt flag.");
    configure_idt();
    sprintf("\n[DripOS] Setting timer speed to 1000 hz.");
    set_pit_freq();
    sprintf("\n[DripOS] Timers set.");

    new_kernel_process("Kernel process", kernel_task);
    sprintf("\n[DripOS] Launched kernel thread, scheduler disabled...");

    sprintf("\n[DripOS] Launching all SMP cores...");
    launch_cpus();
    sprintf("\n[DripOS] Finished loading SMP cores.");

    tty_clear(&base_tty);

    sprintf("\n[DripOS] Loading scheduler...");
    scheduler_enabled = 1;

    while (1) {
        asm volatile("hlt");
    }
}