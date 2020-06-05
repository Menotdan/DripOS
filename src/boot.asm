; Header
%define KERNEL_VMA 0xFFFFFFFF80000000

section .stivalehdr

stivale_header:
    dq stack.top    ; rsp
    dw 1            ; video mode
    dw 0            ; fb_width
    dw 0            ; fb_height
    dw 0            ; bpp
    dq 0            ; custom entry point

section .bss

stack:
    resb 4096
  .top:

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

section .text


; Load the new GDT for the kernel
extern kmain
global exec_start
exec_start:
    lgdt [GDT_PTR_64]

    ; Reload CS
    push 0x10
    push rsp
    pushf
    push 0x8
    push loaded_cs
    iretq

loaded_cs:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov ax, 0x20
    mov gs, ax
    mov fs, ax

    call kmain