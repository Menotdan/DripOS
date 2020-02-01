[bits 64]
section .text
[global long_mode_on]
[extern multiboot_header_pointer]
long_mode_on:
    extern __kernel_end
    mov rsi, __kernel_end ; Kernel end parameter for the kernel to set up memory
    mov edi, DWORD [multiboot_header_pointer]
    extern kmain
    call kmain ; Call the kernel
    cli
hang_loop:
    hlt
    jmp hang_loop