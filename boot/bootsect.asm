[bits 16]

    mov [BOOT_DRIVE], dl ; The BIOS puts the address of the boot drive in the dl register when booting
    mov dh, 2
    mov bx, SECOND_STAGE
    mov cl, 0x02
    call disk_load
    mov bx, BOOT
    call print
    call print_nl
    call clear_screen
    jmp SECOND_STAGE
    jmp $

BOOT db 'Drip OS 0.0012 bootloader, v2', 0
EXIT db 'Second Stage exited, hanging.', 0

%include "boot/print.asm"
%include "boot/hexprint.asm"
%include "boot/disk.asm"
%include "boot/32bit-gdt.asm"
%include "boot/32bit-print.asm"
%include "boot/32bit-switch.asm"

; padding
times 510 - ($-$$) db 0
dw 0xaa55


; Second stage

SECOND_STAGE:
    [bits 16]

    
    KERNEL_OFFSET equ 0xEFFFFF ; The same one we used when linking the kernel
    call switch_to_pm ; disable interrupts, load the GDT,  etc. Finally jumps to 'BEGIN_PM'
    jmp $


    LOADED db 'Second Stage loaded, entering 32 bit mode.', 0
    [bits 32]
    BEGIN_PM:
        mov bx, 64
        mov cx, 0xE0
        mov edx, 0
        call disk_op
        die:
        push edx
        call KERNEL_OFFSET
        push ebx
        mov ebx, WRITE
        call print_string_pm
        pop ebx
        jmp $ ; hang

    %include "boot/32bit-disk.asm"
    %include "boot/32bit-ports.asm"
    [bits 16]
    clear_screen:
        push bp
        mov bp, sp
        pusha

        mov ah, 0x07        ; tells BIOS to scroll down window
        mov al, 0x00        ; clear entire window
        mov bh, 0x07        ; white on black
        mov cx, 0x00        ; specifies top left of screen as (0,0)
        mov dh, 0x18        ; 18h = 24 rows of chars
        mov dl, 0x4f        ; 4fh = 79 cols of chars
        int 0x10            ; calls video interrupt

        popa
        mov sp, bp
        pop bp
        ret
    BOOT_DRIVE db 0 ; It is a good idea to store it in memory because 'dl' may get overwritten
    MSG_PROT_MODE db "32 bit mode loaded!", 0
    MSG_LOAD_KERNEL db "Loading kernel into memory, becuase why not", 0
    MSG_EXIT_KERNEL db "Kernel exited, hanging", 0
    jmp $

times 2560-($-$$) db 0