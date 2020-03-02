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
#include "multiboot.h"

/* Testing includes */
#include "proc/scheduler.h"
#include "io/msr.h"

#include "klibc/string.h"
#include "sys/smp.h"

void kernel_task() {
    kprintf("\n[DripOS] Loading VFS");
    vfs_init(); // Setup VFS
    devfs_init();
    vfs_ops_t ops = dummy_ops;
    ops.open = devfs_open;
    ops.close = devfs_close;
    ops.write = tty_dev_write;
    ops.read = tty_dev_read;
    register_device("tty1", ops);

    int test_dev = fd_open("/dev/tty1", 0);
    char *write = "\nHello from vfs!";
    char *read = kcalloc(10);
    tty_in('a', &base_tty);
    tty_in('b', &base_tty);
    tty_in('c', &base_tty);
    tty_in('d', &base_tty);
    get_thread_locals()->errno = 0;
    fd_write(test_dev, write, strlen(write));
    fd_read(test_dev, read, 4);
    kprintf("\nData read: %s", read);
    sprintf("\nErrno: %d", get_thread_locals()->errno);
    fd_close(test_dev);

    while (1) { asm volatile("hlt"); }
        
}

// Kernel main function, execution starts here :D
void kmain(multiboot_info_t *mboot_dat) {
    init_serial(COM1);

    if (mboot_dat) {
        sprintf("[DripOS]: Setting up memory bitmaps");
        pmm_memory_setup(mboot_dat);
    }

    sprintf("\n[DripOS] Initializing TTY");
    init_vesa(mboot_dat);
    tty_init(&base_tty, 8, 8);

    sprintf("\n[DripOS] Configuring LAPICs and IOAPIC routing");
    configure_apic();

    new_cpu_locals(); // Setup CPU locals for our CPU
    load_tss();
    set_panic_stack((uint64_t) kmalloc(0x1000) + 0x1000);
    set_kernel_stack((uint64_t) kmalloc(0x1000) + 0x1000);
    sprintf("\n[DripOS] Set kernel stacks");
    scheduler_init_bsp();

    sprintf("\n[DripOS] Registering interrupts and setting interrupt flag");
    configure_idt();
    sprintf("\n[DripOS] Setting timer speed to 1000 hz");
    set_pit_freq();

    new_kernel_process("Kernel process", kernel_task);

    launch_cpus();
    tty_clear(&base_tty);

    scheduler_enabled = 1;

    while (1) {
        asm volatile("hlt");
    }
}