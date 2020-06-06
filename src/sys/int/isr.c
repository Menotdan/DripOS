#include "isr.h"
#include "idt.h"
#include "proc/scheduler.h"
#include "proc/urm.h"
#include "drivers/tty/tty.h"
#include "drivers/serial.h"
#include "drivers/pit.h"
#include "drivers/ps2.h"
#include "klibc/stdlib.h"
#include "sys/apic.h"
#include "io/ports.h"
#include "mm/vmm.h"

#include "sys/smp.h"

int_handler_t handlers[IDT_ENTRIES];

void register_int_handler(uint8_t n, int_handler_t handler) {
    handlers[n] = handler;
}

static void send_panic_ipis() {
    madt_ent0_t **cpus = (madt_ent0_t **) vector_items(&cpu_vector);
    for (uint64_t i = 0; i < cpu_vector.items_count; i++) {
        if ((cpus[i]->cpu_flags & 1 || cpus[i]->cpu_flags & 2) && cpus[i]->apic_id != get_lapic_id()) {
            send_ipi(cpus[i]->apic_id, (1 << 14) | 252); // Send interrupt 252
        }
    }
}

void isr_handler(int_reg_t *r) {
    /* If the int number is in range */
    if (r->int_num < IDT_ENTRIES) {
        if (r->int_num < 32) {
            vmm_set_pml4t(base_kernel_cr3); // Use base kernel CR3 in case the alternate CR3 is corrupted
            if (r->cs != 0x1B) {
                /* Exception */
                uint64_t cr2;
                asm volatile("movq %%cr2, %0;" : "=r"(cr2));
                /* Ensure the display is not locked when we crash */
                unlock(base_tty.tty_lock);
                unlock(vesa_lock);

                send_panic_ipis(); // Halt all other CPUs

                if (scheduler_enabled) {
                    safe_kprintf("Exception on core %u with apic id %u! (cur task %s with TID %ld)\n", get_cpu_locals()->cpu_index, get_cpu_locals()->apic_id, get_cpu_locals()->current_thread->name, get_cpu_locals()->current_thread->tid);
                } else {
                    safe_kprintf("Exception on core %u with apic id %u!\n", get_cpu_locals()->cpu_index, get_cpu_locals()->apic_id);
                }
                safe_kprintf("RAX: %lx RBX: %lx RCX: %lx \nRDX: %lx RBP: %lx RDI: %lx \nRSI: %lx R08: %lx R09: %lx \nR10: %lx R11: %lx R12: %lx \nR13: %lx R14: %lx R15: %lx \nRSP: %lx ERR: %lx INT: %lx \nRIP: %lx CR2: %lx CS: %lx\nSS: %lx RFLAGS: %lx\n", r->rax, r->rbx, r->rcx, r->rdx, r->rbp, r->rdi, r->rsi, r->r8, r->r9, r->r10, r->r11, r->r12, r->r13, r->r14, r->r15, r->rsp, r->int_err, r->int_num, r->rip, cr2, r->cs, r->ss, r->rflags);
                if (r->int_num == 14) {
                    safe_kprintf("ERR Code: ");
                    if (r->int_err & (1<<0)) { safe_kprintf("P "); } else { safe_kprintf("NP "); }
                    if (r->int_err & (1<<1)) { safe_kprintf("W "); } else { safe_kprintf("R "); }
                    if (r->int_err & (1<<2)) { safe_kprintf("U "); } else { safe_kprintf("S "); }
                    if (r->int_err & (1<<3)) { safe_kprintf("RES "); }
                }

                while (1) { asm volatile("hlt"); }
            } else {
                uint64_t cr2;
                asm volatile("movq %%cr2, %0;" : "=r"(cr2));

                // Userspace exception
                sprintf("Got userspace exception %lu with error %lu\n", r->int_num, r->int_err);
                sprintf("CR2: %lx RIP %lx\n", cr2, r->rip);
                if (get_cpu_locals()->current_thread->parent_pid) {
                    //sprintf("Killed process %ld\n", get_cpu_locals()->current_thread->parent_pid);
                    interrupt_safe_lock(sched_lock);
                    thread_t *thread = threads[get_cpu_locals()->current_thread->tid];
                    thread->state = BLOCKED;
                    thread->cpu = -1;
                    sprintf("tid = %ld\n", thread->tid);
                    process_t *process = processes[get_cpu_locals()->current_thread->parent_pid];
                    sprintf("killing process %ld with struct address %lx from ISR\n", get_cpu_locals()->current_thread->parent_pid, process);

                    uint64_t size = process->threads_size;
                    int64_t *tids = kmalloc(sizeof(int64_t) * process->threads_size);
                    for (uint64_t i = 0; i < process->threads_size; i++) {
                        tids[i] = process->threads[i];
                    }
                    interrupt_safe_unlock(sched_lock);

                    for (uint64_t i = 0; i < size; i++) {
                        if (tids[i]) {
                            kill_thread(tids[i]);
                        }
                    }

                    interrupt_safe_lock(sched_lock);
                    kfree(processes[get_cpu_locals()->current_thread->parent_pid]);
                    processes[get_cpu_locals()->current_thread->parent_pid] = (void *) 0;
                    interrupt_safe_unlock(sched_lock);
                } else {
                    kill_thread(get_cpu_locals()->current_thread->tid);
                }
                sprintf("Thread is dead\n");
                get_cpu_locals()->current_thread = (thread_t *) 0;
                schedule(r); // Schedule for this CPU
            }
        }
        /* If the entry is present */
        if (handlers[r->int_num]) {
            /* Call the handler */
            handlers[r->int_num](r);
        }
    } else {
        uint64_t cr2;
        asm volatile("movq %%cr2, %0;" : "=r"(cr2));
        sprintf("Bad int no %lu\n", r->int_num);
        sprintf("RAX: %lx RBX: %lx RCX: %lx \nRDX: %lx RBP: %lx RDI: %lx \nRSI: %lx R08: %lx R09: %lx \nR10: %lx R11: %lx R12: %lx \nR13: %lx R14: %lx R15: %lx \nRSP: %lx ERR: %lx INT: %lx \nRIP: %lx CR2: %lx\n", r->rax, r->rbx, r->rcx, r->rdx, r->rbp, r->rdi, r->rsi, r->r8, r->r9, r->r10, r->r11, r->r12, r->r13, r->r14, r->r15, r->rsp, r->int_err, r->int_num, r->rip, cr2);
        while (1) { asm volatile("hlt"); }
    }

    // If we make it here, send an EOI to our LAPIC
    write_lapic(0xB0, 0);
}

void panic_handler(int_reg_t *r) {
    vmm_set_pml4t(base_kernel_cr3); // Use base kernel CR3 in case the alternate CR3 is corrupted

    send_panic_ipis();

    uint64_t cr2;
    asm volatile("movq %%cr2, %0;" : "=r"(cr2));

    safe_kprintf("Panic!\nReason: %s\n", r->rdi);
    if (scheduler_enabled) {
        safe_kprintf("Exception on core %u with apic id %u! (cur task %s with TID %ld)\n", get_cpu_locals()->cpu_index, get_cpu_locals()->apic_id, get_cpu_locals()->current_thread->name, get_cpu_locals()->current_thread->tid);
    } else {
        safe_kprintf("Exception on core %u with apic id %u!\n", get_cpu_locals()->cpu_index, get_cpu_locals()->apic_id);
    }
    safe_kprintf("RAX: %lx RBX: %lx RCX: %lx \nRDX: %lx RBP: %lx RDI: %lx \nRSI: %lx R08: %lx R09: %lx \nR10: %lx R11: %lx R12: %lx \nR13: %lx R14: %lx R15: %lx \nRSP: %lx ERR: %lx INT: %lx \nRIP: %lx CR2: %lx CS: %lx\nSS: %lx RFLAGS: %lx\n", r->rax, r->rbx, r->rcx, r->rdx, r->rbp, r->rdi, r->rsi, r->r8, r->r9, r->r10, r->r11, r->r12, r->r13, r->r14, r->r15, r->rsp, r->int_err, r->int_num, r->rip, cr2, r->cs, r->ss, r->rflags);

    while (1) { asm volatile("hlt"); }
}

void isr_panic_idle(int_reg_t *r) {
    (void) r;
    unlock(base_tty.tty_lock);
    unlock(vesa_lock);
    //kprintf_yieldless("CPU %u, Thread %ld\n", (uint32_t) get_cpu_locals()->cpu_index, get_cpu_locals()->current_thread->tid);
    while (1) { asm volatile("hlt"); }
}

void configure_idt() {
    /* fun stuff like registering 256 interrupt vectors */
    set_idt_gate(0, (uint64_t) isr0);
    set_idt_gate(1, (uint64_t) isr1);
    set_idt_gate(2, (uint64_t) isr2);
    set_idt_gate(3, (uint64_t) isr3);
    set_idt_gate(4, (uint64_t) isr4);
    set_idt_gate(5, (uint64_t) isr5);
    set_idt_gate(6, (uint64_t) isr6);
    set_idt_gate(7, (uint64_t) isr7);
    set_idt_gate(8, (uint64_t) isr8);
    set_idt_gate(9, (uint64_t) isr9);
    set_idt_gate(10, (uint64_t) isr10);
    set_idt_gate(11, (uint64_t) isr11);
    set_idt_gate(12, (uint64_t) isr12);
    set_idt_gate(13, (uint64_t) isr13);
    set_idt_gate(14, (uint64_t) isr14);
    set_idt_gate(15, (uint64_t) isr15);
    set_idt_gate(16, (uint64_t) isr16);
    set_idt_gate(17, (uint64_t) isr17);
    set_idt_gate(18, (uint64_t) isr18);
    set_idt_gate(19, (uint64_t) isr19);
    set_idt_gate(20, (uint64_t) isr20);
    set_idt_gate(21, (uint64_t) isr21);
    set_idt_gate(22, (uint64_t) isr22);
    set_idt_gate(23, (uint64_t) isr23);
    set_idt_gate(24, (uint64_t) isr24);
    set_idt_gate(25, (uint64_t) isr25);
    set_idt_gate(26, (uint64_t) isr26);
    set_idt_gate(27, (uint64_t) isr27);
    set_idt_gate(28, (uint64_t) isr28);
    set_idt_gate(29, (uint64_t) isr29);
    set_idt_gate(30, (uint64_t) isr30);
    set_idt_gate(31, (uint64_t) isr31);
    set_idt_gate(32, (uint64_t) isr32);
    set_idt_gate(33, (uint64_t) isr33);
    set_idt_gate(34, (uint64_t) isr34);
    set_idt_gate(35, (uint64_t) isr35);
    set_idt_gate(36, (uint64_t) isr36);
    set_idt_gate(37, (uint64_t) isr37);
    set_idt_gate(38, (uint64_t) isr38);
    set_idt_gate(39, (uint64_t) isr39);
    set_idt_gate(40, (uint64_t) isr40);
    set_idt_gate(41, (uint64_t) isr41);
    set_idt_gate(42, (uint64_t) isr42);
    set_idt_gate(43, (uint64_t) isr43);
    set_idt_gate(44, (uint64_t) isr44);
    set_idt_gate(45, (uint64_t) isr45);
    set_idt_gate(46, (uint64_t) isr46);
    set_idt_gate(47, (uint64_t) isr47);
    set_idt_gate(48, (uint64_t) isr48);
    set_idt_gate(49, (uint64_t) isr49);
    set_idt_gate(50, (uint64_t) isr50);
    set_idt_gate(51, (uint64_t) isr51);
    set_idt_gate(52, (uint64_t) isr52);
    set_idt_gate(53, (uint64_t) isr53);
    set_idt_gate(54, (uint64_t) isr54);
    set_idt_gate(55, (uint64_t) isr55);
    set_idt_gate(56, (uint64_t) isr56);
    set_idt_gate(57, (uint64_t) isr57);
    set_idt_gate(58, (uint64_t) isr58);
    set_idt_gate(59, (uint64_t) isr59);
    set_idt_gate(60, (uint64_t) isr60);
    set_idt_gate(61, (uint64_t) isr61);
    set_idt_gate(62, (uint64_t) isr62);
    set_idt_gate(63, (uint64_t) isr63);
    set_idt_gate(64, (uint64_t) isr64);
    set_idt_gate(65, (uint64_t) isr65);
    set_idt_gate(66, (uint64_t) isr66);
    set_idt_gate(67, (uint64_t) isr67);
    set_idt_gate(68, (uint64_t) isr68);
    set_idt_gate(69, (uint64_t) isr69);
    set_idt_gate(70, (uint64_t) isr70);
    set_idt_gate(71, (uint64_t) isr71);
    set_idt_gate(72, (uint64_t) isr72);
    set_idt_gate(73, (uint64_t) isr73);
    set_idt_gate(74, (uint64_t) isr74);
    set_idt_gate(75, (uint64_t) isr75);
    set_idt_gate(76, (uint64_t) isr76);
    set_idt_gate(77, (uint64_t) isr77);
    set_idt_gate(78, (uint64_t) isr78);
    set_idt_gate(79, (uint64_t) isr79);
    set_idt_gate(80, (uint64_t) isr80);
    set_idt_gate(81, (uint64_t) isr81);
    set_idt_gate(82, (uint64_t) isr82);
    set_idt_gate(83, (uint64_t) isr83);
    set_idt_gate(84, (uint64_t) isr84);
    set_idt_gate(85, (uint64_t) isr85);
    set_idt_gate(86, (uint64_t) isr86);
    set_idt_gate(87, (uint64_t) isr87);
    set_idt_gate(88, (uint64_t) isr88);
    set_idt_gate(89, (uint64_t) isr89);
    set_idt_gate(90, (uint64_t) isr90);
    set_idt_gate(91, (uint64_t) isr91);
    set_idt_gate(92, (uint64_t) isr92);
    set_idt_gate(93, (uint64_t) isr93);
    set_idt_gate(94, (uint64_t) isr94);
    set_idt_gate(95, (uint64_t) isr95);
    set_idt_gate(96, (uint64_t) isr96);
    set_idt_gate(97, (uint64_t) isr97);
    set_idt_gate(98, (uint64_t) isr98);
    set_idt_gate(99, (uint64_t) isr99);
    set_idt_gate(100, (uint64_t) isr100);
    set_idt_gate(101, (uint64_t) isr101);
    set_idt_gate(102, (uint64_t) isr102);
    set_idt_gate(103, (uint64_t) isr103);
    set_idt_gate(104, (uint64_t) isr104);
    set_idt_gate(105, (uint64_t) isr105);
    set_idt_gate(106, (uint64_t) isr106);
    set_idt_gate(107, (uint64_t) isr107);
    set_idt_gate(108, (uint64_t) isr108);
    set_idt_gate(109, (uint64_t) isr109);
    set_idt_gate(110, (uint64_t) isr110);
    set_idt_gate(111, (uint64_t) isr111);
    set_idt_gate(112, (uint64_t) isr112);
    set_idt_gate(113, (uint64_t) isr113);
    set_idt_gate(114, (uint64_t) isr114);
    set_idt_gate(115, (uint64_t) isr115);
    set_idt_gate(116, (uint64_t) isr116);
    set_idt_gate(117, (uint64_t) isr117);
    set_idt_gate(118, (uint64_t) isr118);
    set_idt_gate(119, (uint64_t) isr119);
    set_idt_gate(120, (uint64_t) isr120);
    set_idt_gate(121, (uint64_t) isr121);
    set_idt_gate(122, (uint64_t) isr122);
    set_idt_gate(123, (uint64_t) isr123);
    set_idt_gate(124, (uint64_t) isr124);
    set_idt_gate(125, (uint64_t) isr125);
    set_idt_gate(126, (uint64_t) isr126);
    set_idt_gate(127, (uint64_t) isr127);
    set_idt_gate(128, (uint64_t) isr128);
    set_idt_gate(129, (uint64_t) isr129);
    set_idt_gate(130, (uint64_t) isr130);
    set_idt_gate(131, (uint64_t) isr131);
    set_idt_gate(132, (uint64_t) isr132);
    set_idt_gate(133, (uint64_t) isr133);
    set_idt_gate(134, (uint64_t) isr134);
    set_idt_gate(135, (uint64_t) isr135);
    set_idt_gate(136, (uint64_t) isr136);
    set_idt_gate(137, (uint64_t) isr137);
    set_idt_gate(138, (uint64_t) isr138);
    set_idt_gate(139, (uint64_t) isr139);
    set_idt_gate(140, (uint64_t) isr140);
    set_idt_gate(141, (uint64_t) isr141);
    set_idt_gate(142, (uint64_t) isr142);
    set_idt_gate(143, (uint64_t) isr143);
    set_idt_gate(144, (uint64_t) isr144);
    set_idt_gate(145, (uint64_t) isr145);
    set_idt_gate(146, (uint64_t) isr146);
    set_idt_gate(147, (uint64_t) isr147);
    set_idt_gate(148, (uint64_t) isr148);
    set_idt_gate(149, (uint64_t) isr149);
    set_idt_gate(150, (uint64_t) isr150);
    set_idt_gate(151, (uint64_t) isr151);
    set_idt_gate(152, (uint64_t) isr152);
    set_idt_gate(153, (uint64_t) isr153);
    set_idt_gate(154, (uint64_t) isr154);
    set_idt_gate(155, (uint64_t) isr155);
    set_idt_gate(156, (uint64_t) isr156);
    set_idt_gate(157, (uint64_t) isr157);
    set_idt_gate(158, (uint64_t) isr158);
    set_idt_gate(159, (uint64_t) isr159);
    set_idt_gate(160, (uint64_t) isr160);
    set_idt_gate(161, (uint64_t) isr161);
    set_idt_gate(162, (uint64_t) isr162);
    set_idt_gate(163, (uint64_t) isr163);
    set_idt_gate(164, (uint64_t) isr164);
    set_idt_gate(165, (uint64_t) isr165);
    set_idt_gate(166, (uint64_t) isr166);
    set_idt_gate(167, (uint64_t) isr167);
    set_idt_gate(168, (uint64_t) isr168);
    set_idt_gate(169, (uint64_t) isr169);
    set_idt_gate(170, (uint64_t) isr170);
    set_idt_gate(171, (uint64_t) isr171);
    set_idt_gate(172, (uint64_t) isr172);
    set_idt_gate(173, (uint64_t) isr173);
    set_idt_gate(174, (uint64_t) isr174);
    set_idt_gate(175, (uint64_t) isr175);
    set_idt_gate(176, (uint64_t) isr176);
    set_idt_gate(177, (uint64_t) isr177);
    set_idt_gate(178, (uint64_t) isr178);
    set_idt_gate(179, (uint64_t) isr179);
    set_idt_gate(180, (uint64_t) isr180);
    set_idt_gate(181, (uint64_t) isr181);
    set_idt_gate(182, (uint64_t) isr182);
    set_idt_gate(183, (uint64_t) isr183);
    set_idt_gate(184, (uint64_t) isr184);
    set_idt_gate(185, (uint64_t) isr185);
    set_idt_gate(186, (uint64_t) isr186);
    set_idt_gate(187, (uint64_t) isr187);
    set_idt_gate(188, (uint64_t) isr188);
    set_idt_gate(189, (uint64_t) isr189);
    set_idt_gate(190, (uint64_t) isr190);
    set_idt_gate(191, (uint64_t) isr191);
    set_idt_gate(192, (uint64_t) isr192);
    set_idt_gate(193, (uint64_t) isr193);
    set_idt_gate(194, (uint64_t) isr194);
    set_idt_gate(195, (uint64_t) isr195);
    set_idt_gate(196, (uint64_t) isr196);
    set_idt_gate(197, (uint64_t) isr197);
    set_idt_gate(198, (uint64_t) isr198);
    set_idt_gate(199, (uint64_t) isr199);
    set_idt_gate(200, (uint64_t) isr200);
    set_idt_gate(201, (uint64_t) isr201);
    set_idt_gate(202, (uint64_t) isr202);
    set_idt_gate(203, (uint64_t) isr203);
    set_idt_gate(204, (uint64_t) isr204);
    set_idt_gate(205, (uint64_t) isr205);
    set_idt_gate(206, (uint64_t) isr206);
    set_idt_gate(207, (uint64_t) isr207);
    set_idt_gate(208, (uint64_t) isr208);
    set_idt_gate(209, (uint64_t) isr209);
    set_idt_gate(210, (uint64_t) isr210);
    set_idt_gate(211, (uint64_t) isr211);
    set_idt_gate(212, (uint64_t) isr212);
    set_idt_gate(213, (uint64_t) isr213);
    set_idt_gate(214, (uint64_t) isr214);
    set_idt_gate(215, (uint64_t) isr215);
    set_idt_gate(216, (uint64_t) isr216);
    set_idt_gate(217, (uint64_t) isr217);
    set_idt_gate(218, (uint64_t) isr218);
    set_idt_gate(219, (uint64_t) isr219);
    set_idt_gate(220, (uint64_t) isr220);
    set_idt_gate(221, (uint64_t) isr221);
    set_idt_gate(222, (uint64_t) isr222);
    set_idt_gate(223, (uint64_t) isr223);
    set_idt_gate(224, (uint64_t) isr224);
    set_idt_gate(225, (uint64_t) isr225);
    set_idt_gate(226, (uint64_t) isr226);
    set_idt_gate(227, (uint64_t) isr227);
    set_idt_gate(228, (uint64_t) isr228);
    set_idt_gate(229, (uint64_t) isr229);
    set_idt_gate(230, (uint64_t) isr230);
    set_idt_gate(231, (uint64_t) isr231);
    set_idt_gate(232, (uint64_t) isr232);
    set_idt_gate(233, (uint64_t) isr233);
    set_idt_gate(234, (uint64_t) isr234);
    set_idt_gate(235, (uint64_t) isr235);
    set_idt_gate(236, (uint64_t) isr236);
    set_idt_gate(237, (uint64_t) isr237);
    set_idt_gate(238, (uint64_t) isr238);
    set_idt_gate(239, (uint64_t) isr239);
    set_idt_gate(240, (uint64_t) isr240);
    set_idt_gate(241, (uint64_t) isr241);
    set_idt_gate(242, (uint64_t) isr242);
    set_idt_gate(243, (uint64_t) isr243);
    set_idt_gate(244, (uint64_t) isr244);
    set_idt_gate(245, (uint64_t) isr245);
    set_idt_gate(246, (uint64_t) isr246);
    set_idt_gate(247, (uint64_t) isr247);
    set_idt_gate(248, (uint64_t) isr248);
    set_idt_gate(249, (uint64_t) isr249);
    set_idt_gate(250, (uint64_t) isr250);
    set_idt_gate(251, (uint64_t) isr251);
    set_idt_gate(252, (uint64_t) isr252);
    set_idt_gate(253, (uint64_t) isr253);
    set_idt_gate(254, (uint64_t) isr254);
    set_idt_gate(255, (uint64_t) isr255);

    /* Set ists */

    /* Panic stacks (IST index 2) */
    set_ist(0, 2);
    set_ist(1, 2);
    set_ist(2, 2);
    set_ist(3, 2);
    set_ist(4, 2);
    set_ist(5, 2);
    set_ist(6, 2);
    set_ist(7, 2);
    set_ist(8, 2);
    set_ist(9, 2);
    set_ist(10, 2);
    set_ist(11, 2);
    set_ist(12, 2);
    set_ist(13, 2);
    set_ist(14, 2);
    set_ist(15, 2);
    set_ist(16, 2);
    set_ist(17, 2);
    set_ist(18, 2);
    set_ist(19, 2);
    set_ist(20, 2);
    set_ist(21, 2);
    set_ist(22, 2);
    set_ist(23, 2);
    set_ist(24, 2);
    set_ist(25, 2);
    set_ist(26, 2);
    set_ist(27, 2);
    set_ist(28, 2);
    set_ist(29, 2);
    set_ist(30, 2);
    set_ist(31, 2);
    set_ist(252, 2);
    set_ist(251, 2);
    /* IRQ Stacks (IST index 1) */
    set_ist(32, 1);
    set_ist(33, 1);
    set_ist(254, 1);
    set_ist(253, 1);

    load_idt(); // Point to the IDT
    register_int_handler(32, timer_handler);
    register_int_handler(33, keyboard_handler);
    register_int_handler(44, mouse_handler);
    register_int_handler(254, schedule);
    register_int_handler(253, schedule_ap);
    register_int_handler(252, isr_panic_idle);
    register_int_handler(251, panic_handler);
    asm volatile("sti"); // Enable interrupts and hope we dont die lmao
}