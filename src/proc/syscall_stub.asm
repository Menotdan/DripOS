[bits 64]

%macro pushaq 0
push rax
push rbx
push rcx
push rdx
push rbp
push rdi
push rsi
push r8
push r9
push r10
push r11
push r12
push r13
push r14
push r15
%endmacro

%macro popaq 0
pop r15
pop r14
pop r13
pop r12
pop r11
pop r10
pop r9
pop r8
pop rsi
pop rdi
pop rbp
pop rdx
pop rcx
pop rbx
pop rax
%endmacro

extern syscall_handler
global syscall_stub

syscall_stub:
    ; Store the user stack and restore the kernel stack
    mov qword [gs:16], rsp
    mov rsp, qword [gs:8]
    pushaq
    cld
    mov rdi, rsp
    call syscall_handler
    popaq
    ; Restore the user stack
    mov rsp, qword [gs:16]
    ; Syscall return
    sysret