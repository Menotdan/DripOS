[bits 32]
; BL: operation, BH: sector count, CX: drive, DX: address
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
    ; Select drive
    push dx
    push ax
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
    pop dx
    mov ax, dx
    push dx
    mov dx, 0x1F3
    call portbout
    ; 
    mov dx, 0x1F6
    mov ax, cx
    call portbout
    pop ax
    pop dx