[bits 64]
section .text
[global long_mode_on]
[extern multiboot_header_pointer]
long_mode_on:
    mov rcx, 0x277
    rdmsr
    ; Clear the PAT entry for PAT1 and set it to WC
    and eax, 0xffff00ff
    or eax, 0x00000100
    wrmsr

    xor rdi, rdi
    mov edi, DWORD [multiboot_header_pointer]
    extern kmain
    call kmain ; Call the kernel
    cli
hang_loop:
    hlt
    jmp hang_loop