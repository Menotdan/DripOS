[bits 64]

section .text

global spinlock_lock
global spinlock_unlock

; To be called like so
; spinlock_lock(uint32_t *lock)
; spinlock_unlock(uint32_t *lock)

spinlock_lock:
    lock bts dword [rdi], 0 ; If the bit is not set already, set it, and set the carry flag if it is set
    jc spin
    ret
spin:
    test dword [rdi], 1 ; This will set the ZF if the bitwise and results in 0
    jnz spin ; If the lock is locked, keep spinning
    jmp spinlock_lock ; If the lock is unlocked, try to acquire it again

spinlock_unlock:
    lock btr dword [rdi], 0 ; Set the bit to 0
    ret