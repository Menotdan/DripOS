global sanity_thread_start
sanity_thread_start:
    mov rax, 12345
    mov rbx, 12345
    mov rcx, 12345
    mov rdx, 12345
    mov rdi, 12345
    mov rsi, 12345
    mov rbp, 12345
    mov r8, 12345
    mov r9, 12345
    mov r10, 12345
    mov r11, 12345
    mov r12, 12345
    mov r13, 12345
    mov r14, 12345
    mov r15, 12345

sanity_thread_loop:
    cmp rax, 12345
    jne sanity_err
    cmp rbx, 12345
    jne sanity_err
    cmp rcx, 12345
    jne sanity_err
    cmp rdx, 12345
    jne sanity_err
    cmp rdi, 12345
    jne sanity_err
    cmp rsi, 12345
    jne sanity_err
    cmp rbp, 12345
    jne sanity_err
    cmp r8, 12345
    jne sanity_err
    cmp r9, 12345
    jne sanity_err
    cmp r10, 12345
    jne sanity_err
    cmp r11, 12345
    jne sanity_err
    cmp r12, 12345
    jne sanity_err
    cmp r13, 12345
    jne sanity_err
    cmp r14, 12345
    jne sanity_err
    cmp r15, 12345
    jne sanity_err

    jmp sanity_thread_loop

extern kprintf    
sanity_err:
    mov rdi, str
    call kprintf
done:
    hlt
    jmp done

str: db 10,"Sanity check failed o-o",0