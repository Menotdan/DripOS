[bits 32]
; D is port, A is data
portbout:
    out dx, al
    ret
portbin:
    in al, dx
    ret
portwout:
    out dx, ax
    ret
portwin:
    in ax, dx
    ret