global memcpy_from_userspace
global memcpy_from_userspace_end
memcpy_from_userspace:
    mov rcx, rdx

    rep movsb

    ret
memcpy_from_userspace_end:


global strcpy_from_userspace
global strcpy_from_userspace_end
strcpy_from_userspace:
    cmp byte [rsi], 0

    je done_strcpy
strcpy_loop:
    cmp rax, 4096   ; Userspace strcpy limit
    je done_err_strcpy

    movsb
    inc rax

    cmp byte [rsi], 0
    jne strcpy_loop
    je done_strcpy
done_err_strcpy:
    mov rax, 0xFFFF ; error ig
done_strcpy:
    ret
strcpy_from_userspace_end:

global strlen_from_userspace
global strlen_from_userspace_end
strlen_from_userspace:
    cmp byte [rsi], 0
    je done_strlen
strlen_loop:
    cmp rax, 4096   ; Userspace strlen limit
    je done_err_strlen

    inc rax
    cmp byte [rsi], 0
    jne strlen_loop
    je done_strlen
done_err_strlen:
    mov rax, 0xFFFF ; error ig
done_strlen:
    ret
strlen_from_userspace_end:

global userspace_func_err
userspace_func_err:
    mov rax, 0xFFFFF ; 5 Fs, not 4
    ret ; Return