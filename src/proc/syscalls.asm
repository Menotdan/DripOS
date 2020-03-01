syscall_stub:
    ; Store the user stack and restore the kernel stack
    mov qword [gs:16], rsp
    mov rsp, qword [gs:8]

    

    ; Restore the user stack
    mov rsp, qword [gs:16]
    ; Syscall return
    sysret