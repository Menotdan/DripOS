[bits 16]

extern GDT64

section .text

global smp_trampoline
global smp_trampoline_end
smp_trampoline:
    cli
    mov word [0x500], 1

    mov eax, cr0
    or eax, 1
    mov cr0, eax
    ; Set up GDT
    lgdt [0x520] ; Load the 32 bit GDT pointer passed by the SMP data
    jmp 0x28:(0x1000 + smp_trampoline_32 - smp_trampoline)

[bits 32]
smp_trampoline_32:
    mov eax, dword [0x510] ; Grab CR3
    mov cr3, eax
    ; Paging

    mov eax, cr4                 ; Set the A-register to control register 4.
    or eax, 1 << 5               ; Set the PAE-bit, which is the 6th bit (bit 5).
    mov cr4, eax                 ; Set control register 4 to the A-register.

    ; Switch to long mode
    mov ecx, 0xC0000080          ; Set the C-register to 0xC0000080, which is the EFER MSR.
    rdmsr                        ; Read from the model-specific register.
    or eax, 1 << 8               ; Set the LM-bit which is the 9th bit (bit 8).
    wrmsr                        ; Write to the model-specific register.
    mov eax, cr0                 ; Set the A-register to control register 0.
    or eax, 1 << 31              ; Set the PG-bit, which is the 32nd bit (bit 31).
    mov cr0, eax                 ; Set control register 0 to the A-register.

    jmp 0x8:(0x1000 + loaded_compatability_mode - smp_trampoline)

[bits 64]
loaded_compatability_mode:
    lgdt [0x530] ; Load the 64 bit gdt pointer passed by the SMP data
    ; Set the segment registers
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax

    ; Setup stack
    mov rsp, qword [0x540]

    mov rax, qword [0x550]
    jmp rax

global long_smp_loaded
extern smp_entry_point
long_smp_loaded:
    mov rcx, 0x277
    rdmsr
    ; Clear the PAT entry for PAT1 and set it to WC
    and eax, 0xffff00ff
    or eax, 0x00000100
    wrmsr

    cld
    xor rax, rax
    call smp_entry_point

smp_trampoline_end: