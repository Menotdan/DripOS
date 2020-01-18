[bits 32]
extern __kernel_start
ALIGN_MULTIBOOT equ 1<<0
MEMINFO equ 1<<1
VIDEO_MODE equ 0x00000004
FLAGS equ ALIGN_MULTIBOOT | MEMINFO | VIDEO_MODE
MAGIC equ 0x1BADB002
CHECKSUM equ -(MAGIC + FLAGS)
VIDMODE equ 0
WIDTH equ 1280
HEIGHT equ 720
DEPTH equ 32

section .multiboot
align 4
dd MAGIC
dd FLAGS
dd CHECKSUM
dd 0
dd 0
dd 0
dd 0
dd 0
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

GDT64:                           ; Global Descriptor Table (64-bit).
    .Null: equ $ - GDT64         ; The null descriptor.
    dw 0xFFFF                    ; Limit (low).
    dw 0                         ; Base (low).
    db 0                         ; Base (middle)
    db 0                         ; Access.
    db 0                         ; Granularity.
    db 0                         ; Base (high).
    .Code: equ $ - GDT64         ; The code descriptor.
    dw 0                         ; Limit (low).
    dw 0                         ; Base (low).
    db 0                         ; Base (middle)
    db 10011010b                 ; Access (exec/read).
    db 10101111b                 ; Granularity, 64 bits flag, limit19:16.
    db 0                         ; Base (high).
    .Data: equ $ - GDT64         ; The data descriptor.
    dw 0                         ; Limit (low).
    dw 0                         ; Base (low).
    db 0                         ; Base (middle)
    db 10010010b                 ; Access (read/write).
    db 00000000b                 ; Granularity.
    db 0                         ; Base (high).
    .Pointer:                    ; The GDT-pointer.
    dw $ - GDT64 - 1             ; Limit.
    dq GDT64                     ; Base.
    .Pointer32:                    ; The GDT-pointer for 32 bit mode.
    dw $ - GDT64 - 1             ; Limit.
    dd GDT64 - 0xffffffff80000000; Base.

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
    dq (pdpt - 0xffffffff80000000 + 0x3)
    times 255 dq 0
    dq (pdpt2 - 0xffffffff80000000 + 0x3)
    times 254 dq 0
    dq (pdpt3 - 0xffffffff80000000 + 0x3)

align 4096
pdpt:
    dq (paging_directory1 - 0xffffffff80000000 + 0x3)
    times 511 dq 0

align 4096
pdpt2:
    dq (paging_directory2 - 0xffffffff80000000 + 0x3)
    times 511 dq 0

align 4096
pdpt3:
    times 510 dq 0
    dq (paging_directory3 - 0xffffffff80000000 + 0x3)
    dq (paging_directory4 - 0xffffffff80000000 + 0x3)

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
    mov esp, stack_top - 0xffffffff80000000 ; Set the stack up
    mov edi, multiboot_header_pointer - 0xffffffff80000000
    mov DWORD [edi], ebx
    mov eax, pml4t - 0xffffffff80000000
    mov cr3, eax
    ; Paging

    mov eax, cr4                 ; Set the A-register to control register 4.
    or eax, 1 << 5               ; Set the PAE-bit, which is the 6th bit (bit 5).
    ;or eax, 1 << 4               ; Set the PSE-bit, which is the 5th bit (bit 4).
    mov cr4, eax                 ; Set control register 4 to the A-register.

    ; Switch to long mode
    mov ecx, 0xC0000080          ; Set the C-register to 0xC0000080, which is the EFER MSR.
    rdmsr                        ; Read from the model-specific register.
    or eax, 1 << 8               ; Set the LM-bit which is the 9th bit (bit 8).
    wrmsr                        ; Write to the model-specific register.
    mov eax, cr0                 ; Set the A-register to control register 0.
    or eax, 1 << 31              ; Set the PG-bit, which is the 32nd bit (bit 31).
    mov cr0, eax                 ; Set control register 0 to the A-register.

    mov eax, 1234

    ;cli
    ;hlt

    ; Set up GDT
    lgdt [GDT64.Pointer32 - 0xffffffff80000000]
    jmp GDT64.Code:loaded - 0xffffffff80000000

[bits 64]
loaded:
    lgdt [GDT64.Pointer]         ; Load the 64-bit global descriptor table.
    mov ax, GDT64.Data           ; Set the A-register to the data descriptor.
    mov ds, ax                    ; Set the data segment to the A-register.
    mov es, ax                    ; Set the extra segment to the A-register.
    mov fs, ax                    ; Set the F-segment to the A-register.
    mov gs, ax                    ; Set the G-segment to the A-register.
    mov ss, ax                    ; Set the stack segment to the A-register.
    ; Perform an absolute jump
    mov rax, long_mode_on
    jmp rax