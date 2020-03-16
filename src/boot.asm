[bits 32]

extern __kernel_start
%define KERNEL_VMA 0xFFFFFFFF80000000

; Flag constants
ALIGN_MULTIBOOT  equ 1<<0
MEMINFO          equ 1<<1
VIDEO_MODE       equ 1<<2
; Magic, flags, and checksum
FLAGS            equ ALIGN_MULTIBOOT | MEMINFO | VIDEO_MODE
MAGIC            equ 0x1BADB002
CHECKSUM         equ -(MAGIC + FLAGS)
; Video mode data
VIDMODE          equ 0
WIDTH            equ 1280
HEIGHT           equ 720
DEPTH            equ 32

section .multiboot
align 4
; Magic, flags, and checksum
dd MAGIC
dd FLAGS
dd CHECKSUM
; Unused data
dd 0
dd 0
dd 0
dd 0
dd 0
; Video mode data
dd VIDMODE
dd WIDTH
dd HEIGHT
dd DEPTH

%macro gen_pd_2mb 3
    %assign i %1
    %rep %2
        dq (i | 0x83)
        %assign i i+0x200000
    %endrep
    %rep %3
        dq 0
    %endrep
%endmacro

section .data

align 4096
global GDT64
GDT64:                           ; Global Descriptor Table (64-bit).
    .Null: equ $ - GDT64         ; The null descriptor.
    dw 0xFFFF                    ; Limit (low).
    dw 0                         ; Base (low).
    db 0                         ; Base (middle)
    db 0                         ; Access.
    db 0                         ; Granularity.
    db 0                         ; Base (high).
    .Code: equ $ - GDT64         ; The code descriptor.
    dw 0xFFFF                    ; Limit (low).
    dw 0                         ; Base (low).
    db 0                         ; Base (middle)
    db 10011011b                 ; Access (exec/read).
    db 10101111b                 ; Granularity, 64 bits flag, limit19:16.
    db 0                         ; Base (high).
    .Data: equ $ - GDT64         ; The data descriptor.
    dw 0xFFFF                    ; Limit (low).
    dw 0                         ; Base (low).
    db 0                         ; Base (middle)
    db 10010011b                 ; Access (read/write).
    db 11001111b                 ; Granularity.
    db 0                         ; Base (high).
    .UserCode: equ $ - GDT64     ; The user code descriptor.
    dw 0xFFFF                    ; Limit (low).
    dw 0                         ; Base (low).
    db 0                         ; Base (middle)
    db 11111101b                 ; Access (exec/read).
    db 10101111b                 ; Granularity, 64 bits flag, limit19:16.
    db 0                         ; Base (high).
    .UserData: equ $ - GDT64     ; The user data descriptor.
    dw 0xFFFF                    ; Limit (low).
    dw 0                         ; Base (low).
    db 0                         ; Base (middle)
    db 11110011b                 ; Access (read/write).
    db 11001111b                 ; Granularity.
    db 0                         ; Base (high).
    .Code32: equ $ - GDT64       ; The 32 bit code descriptor for SMP core booting.
    dq 0x00CF9A000000FFFF
    .TSS_LOAD_SEG: equ $ - GDT64
    dq 0x0
    dq 0x0

global GDT_PTR_32
global GDT_PTR_64

GDT_PTR_32:
    dw $ - GDT64 - 1             ; Limit.
    dd GDT64 - KERNEL_VMA        ; Base.

GDT_PTR_64:
    dw GDT_PTR_32 - GDT64 - 1    ; Limit.
    dq GDT64                     ; Base.

section .bss
align 16
stack_bottom:
resb 65536
stack_top:

section .data
align 4096
paging_directory1:
    gen_pd_2mb 0, 12, 500

align 4096
paging_directory2:
    gen_pd_2mb 0, 512, 0

align 4096
paging_directory3:
    gen_pd_2mb 0x0, 512, 0

align 4096
paging_directory4:
    gen_pd_2mb 0x40000000, 512, 0

align 4096
pml4t:
    dq (pdpt - KERNEL_VMA + 0x3)
    times 255 dq 0
    dq (pdpt2 - KERNEL_VMA + 0x3)
    times 254 dq 0
    dq (pdpt3 - KERNEL_VMA + 0x3)

align 4096
pdpt:
    dq (paging_directory1 - KERNEL_VMA + 0x3)
    times 511 dq 0

align 4096
pdpt2:
    dq (paging_directory2 - KERNEL_VMA + 0x3)
    times 511 dq 0

align 4096
pdpt3:
    times 510 dq 0
    dq (paging_directory3 - KERNEL_VMA + 0x3)
    dq (paging_directory4 - KERNEL_VMA + 0x3)

section .bss
global multiboot_header_pointer
multiboot_header_pointer:
resb 4

section .text
extern long_mode_on
extern __kernel_end
extern paging_setup
global _start
_start:
    mov edi, multiboot_header_pointer - KERNEL_VMA
    mov DWORD [edi], ebx
    mov eax, pml4t - KERNEL_VMA
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

    ; Set up GDT
    lgdt [GDT_PTR_32 - KERNEL_VMA]
    jmp GDT64.Code:loaded - KERNEL_VMA

[bits 64]
loaded:
    lgdt [GDT_PTR_64]           ; Load the 64-bit global descriptor table.
    mov ax, GDT64.Data
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov ax, 0x20
    mov gs, ax
    mov fs, ax
    mov rsp, stack_top ; Set the stack up
    ; Perform an absolute jump
    mov rax, long_mode_on
    jmp rax