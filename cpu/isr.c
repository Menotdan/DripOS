#include <serial.h>
#include <libc.h>
#include "isr.h"
#include "idt.h"
#include "../drivers/screen.h"
#include "../drivers/keyboard.h"
#include <string.h>
#include "timer.h"
#include "ports.h"
#include "../fs/hdd.h"
#include "../kernel/kernel.h"
#include "task.h"
#include <debug.h>

isr_t interrupt_handlers[256];
isr_t handler;
registers_t *data;

/* Can't do this with a loop because we need the address
 * of the function names */
void isr_install() {
    set_idt_gate(0, (uint64_t)isr0);
    set_idt_gate(1, (uint64_t)isr1);
    set_idt_gate(2, (uint64_t)isr2);
    set_idt_gate(3, (uint64_t)isr3);
    set_idt_gate(4, (uint64_t)isr4);
    set_idt_gate(5, (uint64_t)isr5);
    set_idt_gate(6, (uint64_t)isr6);
    set_idt_gate(7, (uint64_t)isr7);
    set_idt_gate(8, (uint64_t)isr8);
    set_idt_gate(9, (uint64_t)isr9);
    set_idt_gate(10, (uint64_t)isr10);
    set_idt_gate(11, (uint64_t)isr11);
    set_idt_gate(12, (uint64_t)isr12);
    set_idt_gate(13, (uint64_t)isr13);
    set_idt_gate(14, (uint64_t)isr14);
    set_idt_gate(15, (uint64_t)isr15);
    set_idt_gate(16, (uint64_t)isr16);
    set_idt_gate(17, (uint64_t)isr17);
    set_idt_gate(18, (uint64_t)isr18);
    set_idt_gate(19, (uint64_t)isr19);
    set_idt_gate(20, (uint64_t)isr20);
    set_idt_gate(21, (uint64_t)isr21);
    set_idt_gate(22, (uint64_t)isr22);
    set_idt_gate(23, (uint64_t)isr23);
    set_idt_gate(24, (uint64_t)isr24);
    set_idt_gate(25, (uint64_t)isr25);
    set_idt_gate(26, (uint64_t)isr26);
    set_idt_gate(27, (uint64_t)isr27);
    set_idt_gate(28, (uint64_t)isr28);
    set_idt_gate(29, (uint64_t)isr29);
    set_idt_gate(30, (uint64_t)isr30);
    set_idt_gate(31, (uint64_t)isr31);

    // Remap the PIC
    port_byte_out(0x20, 0x11);
    port_byte_out(0xA0, 0x11);
    port_byte_out(0x21, 0x20);
    port_byte_out(0xA1, 0x28);
    port_byte_out(0x21, 0x04);
    port_byte_out(0xA1, 0x02);
    port_byte_out(0x21, 0x01);
    port_byte_out(0xA1, 0x01);
    port_byte_out(0x21, 0x0);
    port_byte_out(0xA1, 0x0); 

    // Install the IRQs
    set_idt_gate(32, (uint64_t)irq0);
    set_idt_gate(33, (uint64_t)irq1);
    set_idt_gate(34, (uint64_t)irq2);
    set_idt_gate(35, (uint64_t)irq3);
    set_idt_gate(36, (uint64_t)irq4);
    set_idt_gate(37, (uint64_t)irq5);
    set_idt_gate(38, (uint64_t)irq6);
    set_idt_gate(39, (uint64_t)irq7);
    set_idt_gate(40, (uint64_t)irq8);
    set_idt_gate(41, (uint64_t)irq9);
    set_idt_gate(42, (uint64_t)irq10);
    set_idt_gate(43, (uint64_t)irq11);
    set_idt_gate(44, (uint64_t)irq12);
    set_idt_gate(45, (uint64_t)irq13);
    set_idt_gate(46, (uint64_t)irq14);
    set_idt_gate(47, (uint64_t)irq15);
    
    // Syscall handler
    set_idt_gate(0x80, (uint64_t)sys);

    set_idt(); // Load with ASM
}

/* To print the message which defines every exception */
char *exception_messages[] = {
    "Division By Zero",
    "Debug",
    "Non Maskable Interrupt",
    "Breakpoint",
    "Into Detected Overflow",
    "Out of Bounds",
    "Invalid Opcode",
    "No Coprocessor",

    "Double Fault",
    "Coprocessor Segment Overrun",
    "Bad TSS",
    "Segment Not Present",
    "Stack Fault",
    "General Protection Fault",
    "Page Fault",
    "Unknown Interrupt",

    "Coprocessor Fault",
    "Alignment Check",
    "Machine Check",
    "Reserved",
    "Assert Error",
    "Out of memory",
    "Reserved",
    "Reserved",

    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved"
};

void isr_handler(registers_t *r) {
    //sprint("\nCalls to switch: ");
    //sprint_uint(global_esp);
    //crash_screen(r, exception_messages[r->int_no], 1);
    sprint("\nException! Number: ");
    sprint_uint(r->int_no);
    sprint(" Message: ");
    sprint(exception_messages[r->int_no]);
    uint64_t cr2;
    asm volatile("movq %%cr2, %0;":"=r"(cr2));
    sprintf("\nRIP: %lx CR2: %lx ERR: %lu\n", r->rip, cr2, r->err_code);
    clear_screen();
    kprintf("\nKernel panic!\nRAX: %lx RBX: %lx RCX: %lx\nRDX: %lx RDI: %lx RSI: %lx\nR08: %lx R09: %lx R10: %lx\nR11: %lx R12: %lx R13: %lx\nR14: %lx R15: %lx RBP: %lx\nRSP: %lx RIP: %lx FLG: %lx", r->rax, r->rbx, r->rcx, r->rdx, r->rdi, r->rsi, r->r8, r->r9, r->r10, r->r11, r->r12, r->r13, r->r14, r->r15, r->rbp, r->rsp, r->rip, r->rflags);
    kprintf("\n\n\n%lx %lx %lx\n%lx %lx %lx\n%lx\n\n\n\n\nActual dump:\n%lx %lx %lx\n%lx %lx %lx\n%lx %lx %lx\n%lx %lx %lx\n%lx %lx %lx\n%lx %lx %lx\n%lx %lx %lx\n%lx %lx %lx\n%lx %lx %lx\n%lx %lx %lx\n%lx %lx %lx\n%lx %lx %lx\n%lx %lx %lx\n%lx %lx %lx\n%lx %lx %lx\n%lx %lx %lx\n%lx %lx %lx\n%lx %lx %lx\n%lx %lx %lx\n%lx %lx %lx\n%lx %lx %lx\n%lx %lx %lx\n%lx %lx %lx\n%lx %lx %lx\n%lx %lx %lx\n%lx %lx %lx\n");
    while (1);
}

void register_interrupt_handler(uint8_t n, isr_t handler) {
    interrupt_handlers[n] = handler;
}

void irq_handler(registers_t *r) {
    /* Handle the interrupt in a more modular way */
    if (interrupt_handlers[r->int_no] != 0) {
        handler = interrupt_handlers[r->int_no];
        handler(r);
        if (loaded == 1) {
            Task *iterator = (&main_task)->next;
            while (iterator->pid != 0) {
                /* Set all the tasks waiting for this IRQ to Running */
                if (iterator->state == IRQ_WAIT) {
                    if (iterator->waiting == (r->int_no-32)) {
                        iterator->state = RUNNING;
                    }
                }
                iterator = iterator->next;
            }
        }
    } 
    else {
        if (loaded == 1) {
            kprint("");
        }
    }
    /* After every interrupt we need to send an EOI to the PICs
     * or they will not send another interrupt again */

    if (r->int_no >= 40) port_byte_out(0xA0, 0x20); /* slave */
    port_byte_out(0x20, 0x20); /* master */
}

void irq_install() {
    /* Enable interruptions */
    asm volatile("sti");
    //init_hdd();
}