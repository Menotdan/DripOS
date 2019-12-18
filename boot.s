/* Enable intel syntax */
.intel_syntax noprefix
/* Declare constants for the multiboot header. */
.set ALIGN,    1<<0             /* align loaded modules on page boundaries */
.set MEMINFO,  1<<1             /* provide memory map */
.set VIDEO_MODE, 0x00000004           /* set video mode */
.set FLAGS, ALIGN | MEMINFO | VIDEO_MODE    /* this is the Multiboot 'flag' field */
.set MAGIC,    0x1BADB002       /* 'magic number' lets bootloader find the header */
.set CHECKSUM, -(MAGIC + FLAGS) /* checksum of above, to prove we are multiboot */
.set VIDMODE, 0
.set WIDTH, 720
.set HEIGHT, 480
.set DEPTH, 32

/*
Declare a multiboot header that marks the program as a kernel. These are magic
values that are documented in the multiboot standard. The bootloader will
search for this signature in the first 8 KiB of the kernel file, aligned at a
32-bit boundary. The signature is in its own section so the header can be
forced to be within the first 8 KiB of the kernel file.
*/
.section .multiboot
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM
.long 0, 0, 0, 0, 0
.long VIDMODE
.long WIDTH
.long HEIGHT
.long DEPTH

.section .data
/*
GDT from the old DripOS bootloader, which was from the original
project (The OS tutorial)
*/
 
gdt_start:
 
        .long 0x0
        .long 0x0
 
gdt_code:
        .word 0xffff
        .word 0x0
        .byte 0x0
        .byte 0x9A /*10011010 in binary*/
        .byte 0xCF /*11001111 in binary*/
        .byte 0x0
gdt_data:
        .word 0xffff
        .word 0x0
        .byte 0x0
        .byte 0x92 /*10010010 in binary*/
        .byte 0xCF /*11001111 in binary*/
        .byte 0x0
 
gdt_end:
 
gdt_descriptor:
        .word gdt_end - gdt_start - 1
        .long gdt_start
 
CODE_SEG = gdt_code - gdt_start
DATA_SEG = gdt_data - gdt_start
 
/*
The multiboot standard does not define the value of the stack pointer register
(esp) and it is up to the kernel to provide a stack. This allocates room for a
small stack by creating a symbol at the bottom of it, then allocating 16384
bytes for it, and finally creating a symbol at the top. The stack grows
downwards on x86. The stack is in its own section so it can be marked nobits,
which means the kernel file is smaller because it does not contain an
uninitialized stack. The stack on x86 must be 16-byte aligned according to the
System V ABI standard and de-facto extensions. The compiler will assume the
stack is properly aligned and failure to align the stack will result in
undefined behavior.
*/
.section .bss
.align 16
.global stack_top
stack_bottom:
.skip 65536 /* 64 KiB of kernel stack, stack for other processes will
allocated when they are created */
stack_top:

/*
The linker script specifies _start as the entry point to the kernel and the
bootloader will jump to this position once the kernel has been loaded. It
doesn't make sense to return from this function as the bootloader is gone.
*/
.section .text
.global _start
.type _start, @function
_start:
        /*
        The bootloader has loaded us into 32-bit protected mode on a x86
        machine. Interrupts are disabled. Paging is disabled. The processor
        state is as defined in the multiboot standard. The kernel has full
        control of the CPU. The kernel can only make use of hardware features
        and any code it provides as part of itself. There's no printf
        function, unless the kernel provides its own <stdio.h> header and a
        printf implementation. There are no security restrictions, no
        safeguards, no debugging mechanisms, only what the kernel provides
        itself. It has absolute and complete power over the
        machine.
        */
 
        /*
        To set up a stack, we set the esp register to point to the top of the
        stack (as it grows downwards on x86 systems). This is necessarily done
        in assembly as languages such as C cannot function without a stack.
        */
        mov stack_top, esp
 
        /*
        This is a good place to initialize crucial processor state before the
        high-level kernel is entered. It's best to minimize the early
        environment where crucial features are offline. Note that the
        processor is not fully initialized yet: Features such as floating
        point instructions and instruction set extensions are not initialized
        yet. The GDT should be loaded here. Paging should be enabled here.
        C++ features such as global constructors and exceptions will require
        runtime support to work as well.
        */
        lgdt [gdt_descriptor] /* Load the GDT */
        /*
        Enter the high-level kernel. The ABI requires the stack is 16-byte
        aligned at the time of the call instruction (which afterwards pushes
        the return pointer of size 4 bytes). The stack was originally 16-byte
        aligned above and we've since pushed a multiple of 16 bytes to the
        stack since (pushed 0 bytes so far) and the alignment is thus
        preserved and the call is well defined.
        */
		/* Credit goes to Michael Petch on StackOverflow for helping correctly write this*/
    mov ax, DATA_SEG
        mov ds, ax
        mov es, ax
        mov fs, ax
        mov gs, ax
    jmp CODE_SEG:.next /* JMP to next instruction but set CS! */
.next:
        mov edx, [__kernel_end]
        .intel_syntax noprefix
        mov eax, ebx
        /*mov ebp, 0x90000
        mov esp, ebp*/
        call kmain
 
        /*
        If the system has nothing more to do, put the computer into an
        infinite loop. To do that:
        1) Disable interrupts with cli (clear interrupt enable in eflags).
           They are already disabled by the bootloader, so this is not needed.
           Mind that you might later enable interrupts and return from
           kernel_main (which is sort of nonsensical to do).
        2) Wait for the next interrupt to arrive with hlt (halt instruction).
           Since they are disabled, this will lock up the computer.
        3) Jump to the hlt instruction if it ever wakes up due to a
           non-maskable interrupt occurring or due to system management mode.
        */
        cli
1:      hlt
        jmp 1b
 
/*
Set the size of the _start symbol to the current location '.' minus its start.
This is useful when debugging or when you implement call tracing.
*/
.size _start, . - _start