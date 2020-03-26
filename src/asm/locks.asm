[bits 64]

section .text

global spinlock_lock
global spinlock_unlock
global atomic_inc
global atomic_dec

; To be called like so
; spinlock_lock(uint32_t *lock)
; spinlock_unlock(uint32_t *lock)

spinlock_lock:
    lock bts dword [rdi], 0 ; If the bit is not set already, set it, and set the carry flag if it is set
    jc spin
    ret
spin:
    pause
    test dword [rdi], 1 ; This will set the ZF if the bitwise and results in 0
    jnz spin ; If the lock is locked, keep spinning
    jmp spinlock_lock ; If the lock is unlocked, try to acquire it again

spinlock_unlock:
    lock btr dword [rdi], 0 ; Set the bit to 0
    ret

atomic_inc:
    lock inc dword [rdi]
    pushf
    pop rax
    not rax
    and rax, (1<<6)
    shr rax, 6
    ret

atomic_dec:
    lock dec dword [rdi]
    pushf
    pop rax
    not rax
    and rax, (1<<6)
    shr rax, 6
    ret
