[bits 32]
; BX: sector count, CX: drive, EDX: address
; 0x1F0: Data
; 0x1F1: Error/Features
; 0x1F2: Sector Count
; 0x1F3: Sector Number
; 0x1F4: Cyl LOW
; 0x1F5: Cyl HIGH
; 0x1F6: Drive select
; 0x1F7: Status/Command
; 0x3F6: Alt status
disk_op:
    ; push used params onto the stack
    push ax
    push edx
    ; Select drive
    mov dx, 0x1F6
    mov ax, cx
    call portbout
    ; Select Features
    mov dx, 0x1F1
    mov ax, 0x00
    call portbout
    ; Set sector count
    mov dx, 0x1F2
    movzx ax, bh
    call portbout
    ; Select start sector
    pop edx
    mov eax, edx
    push edx
    mov dx, 0x1F3
    call portbout
    ; Set cyl low
    pop edx
    mov eax, edx
    push edx
    shr eax, 8
    mov dx, 0x1F4
    call portbout
    ; Set cyl high
    pop edx
    mov eax, edx
    push edx
    shr eax, 16
    mov dx, 0x1F4
    call portbout
    ; pop used params off the stack
    pop edx
    pop ax
    jmp select_read
    mov dx, 0x3F6
    handled:
    ; return to caller
    ret


drq_set:
    call portbin
    call portbin
    call portbin
    call portbin
    call portbin
    and al, 0x01
    cmp al, 0x01
    jne read_data
    push ebx
    mov ebx, ERR_BLOCK
    call print_string_pm
    pop ebx
    jmp handled

no_drq:
    call portbin
    call portbin
    call portbin
    call portbin
    call portbin
    call portbin
    call portbin
    call portbin
    call portbin
    and al, 0x01
    cmp al, 0x01
    je error_both
    push ebx
    mov ebx, ERR_CYCLE
    call print_string_pm
    pop ebx
    jmp handled

error_both:
    push ebx
    mov ebx, ERR_BOTH
    call print_string_pm
    pop ebx
    jmp handled

read_data:
    push ecx
    push ebx
    push eax
    mov ecx, 0x0
    mov ebx, 0xEFFFFF
    jmp read_loop
    return_label:


read_loop:
    mov dx, 0x1F0
    call portwin
    mov [ebx], ax
    add ecx, 0x2
    add ebx, 0x2
    cmp ecx, 0x200
    je inc_sector_count


inc_sector_count:
    pop eax
    inc eax
    pop ebx
    cmp bx, ax
    je handled
    push ebx
    push eax
    jmp read_loop

select_read:
    push ax
    push dx
    mov dx, 0x1F7
    mov ax, 0x20
    call portbout
    pop dx
    pop ax
    mov ecx, 0
    jmp check_drq

check_drq:
    call portbin ; reading ports for delay
    call portbin
    call portbin
    call portbin
    call portbin
    and al, 0x88
    cmp al, 0x08
    je drq_set
    inc ecx
    cmp ecx, 1000
    jne check_drq
    jne no_drq

WRITE db "Write selected     ", 0
READ db "Read selected      ", 0
ERR_CYCLE db "Bad Cycle!",0
ERR_BLOCK db "Bad block!",0
ERR_BOTH db "Bad Block! Bad Cycle!",0