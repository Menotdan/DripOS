; Defined in isr.c
[bits 64]
[extern isr_handler]
[extern irq_handler]
[extern syscall_handle]
[extern switch_task]
[extern global_esp]
[extern global_esp_old]
[extern swap_task]
[extern schedule_task]
[extern breakA]
[extern free]
[extern global_old_task]
[extern task_size]
[extern sprintf]

%macro pushaq 0
    push rax      ; save current rax
    push rbx      ; save current rbx
    push rcx      ; save current rcx
    push rdx      ; save current rdx
    push rbp      ; save current rbp
    push rdi      ; save current rdi
    push rsi      ; save current rsi
    push r8       ; save current r8
    push r9       ; save current r9
    push r10      ; save current r10
    push r11      ; save current r11
    push r12      ; save current r12
    push r13      ; save current r13
    push r14      ; save current r14
    push r15      ; save current r15
%endmacro

%macro popaq 0
    pop r15       ; restore current r15
    pop r14       ; restore current r14
    pop r13       ; restore current r13
    pop r12       ; restore current r12
    pop r11       ; restore current r11
    pop r10       ; restore current r10
    pop r9        ; restore current r9
    pop r8        ; restore current r8
    pop rsi       ; restore current rsi
    pop rdi       ; restore current rdi
    pop rbp       ; restore current rbp
    pop rdx       ; restore current rdx
    pop rcx       ; restore current rcx
    pop rbx       ; restore current rbx
    pop rax       ; restore current rax
%endmacro

section .rodata
string db 10,'Switching task',0 ; "\nSwitching task"
string_printf db 10,'Switch_task var: %u',0

section .text

; Common ISR code
isr_common_stub:
    ; 1. Save CPU state
    pushaq
    mov ax, ds ; Lower 16-bits of eax = ds.
    push rax ; save the data segment descriptor
    mov rax, dr6
    push rax
    mov rdi, rsp

    ; 2. Call C handler
    ;cld
    call isr_handler
    add rsp, 16
    popaq
    ;pop esp
    add rsp, 16 ; Cleans up the pushed error code and pushed ISR number
    iretq ; pops 5 things at once: CS, EIP, EFLAGS, SS, and ESP
    

; Common IRQ code. Identical to ISR code except for the 'call' 
; and the 'pop ebx'
irq_common_stub:
    pushaq
    mov ax, ds
    push rax

    mov rax, dr6
    push rax
    mov rdi, rsp                 ; At this point ESP is a pointer to where DS (and the rest
                             ; of the interrupt handler state resides)
                             ; Push ESP as 1st parameter as it's a 
                             ; pointer to a registers_t
    cld
    call irq_handler
    ;mov rdi, string_printf
    ;mov rsi, [switch_task]
    ;call sprintf
    mov rbx, [switch_task]
    cmp rbx, 1
    jne testLabel
    xor rbx, rbx
    mov [switch_task], rbx

    ;mov rdi, string
    ;call sprintf

    mov rdi, rsp ; Param
    call swap_task ; Changes the interrupt frame
testLabel:
    add rsp, 16 ; DR6 and ss

    popaq
    add rsp, 16 ; IRQ code and error code
    iretq ; Ret

syscall_handler:
    ; 1. Save CPU state
    pushaq ; Pushes edi,esi,ebp,esp,ebx,edx,ecx,eax
    mov ax, ds ; Lower 16-bits of eax = ds.
    push rax ; save the data segment descriptor
    mov rax, dr6
    push rax
    mov rdi, rsp

    ; 2. Call C handler
    cld
    call syscall_handle
    mov rbx, [switch_task]
    cmp rbx, 1
    jne no_switch
    xor rbx, rbx
    mov [switch_task], rbx
    
    mov rdi, rsp ; Set parameter
    call swap_task ; Swap out values on the stack
no_switch:
    add rsp, 16
    popaq
    add rsp, 16 ; Cleans up the pushed error code and pushed ISR number
    iretq ; pops 3 things at once: CS, EIP, EFLAGS

return_stub:

; We don't get information about which interrupt was caller
; when the handler is run, so we will need to have a different handler
; for every interrupt.
; Furthermore, some interrupts push an error code onto the stack but others
; don't, so we will push a dummy error code for those which don't, so that
; we have a consistent stack for all of them.

; First make the ISRs global
global isr0
global isr1
global isr2
global isr3
global isr4
global isr5
global isr6
global isr7
global isr8
global isr9
global isr10
global isr11
global isr12
global isr13
global isr14
global isr15
global isr16
global isr17
global isr18
global isr19
global isr20
global isr21
global isr22
global isr23
global isr24
global isr25
global isr26
global isr27
global isr28
global isr29
global isr30
global isr31
; IRQs
global irq0
global irq1
global irq2
global irq3
global irq4
global irq5
global irq6
global irq7
global irq8
global irq9
global irq10
global irq11
global irq12
global irq13
global irq14
global irq15
; Syscall
global sys

; 0: Divide By Zero Exception
isr0:
    cli
    push byte 0
    push byte 0
    jmp isr_common_stub

; 1: Debug Exception
isr1:
    cli
    push byte 0
    push byte 1
    jmp isr_common_stub

; 2: Non Maskable Interrupt Exception
isr2:
    cli
    push byte 0
    push byte 2
    jmp isr_common_stub

; 3: Int 3 Exception
isr3:
    cli
    push byte 0
    push byte 3
    jmp isr_common_stub

; 4: INTO Exception
isr4:
    cli
    push byte 0
    push byte 4
    jmp isr_common_stub

; 5: Out of Bounds Exception
isr5:
    cli
    push byte 0
    push byte 5
    jmp isr_common_stub

; 6: Invalid Opcode Exception
isr6:
    cli
    push byte 0
    push byte 6
    jmp isr_common_stub

; 7: Coprocessor Not Available Exception
isr7:
    cli
    push byte 0
    push byte 7
    jmp isr_common_stub

; 8: Double Fault Exception (With Error Code!)
isr8:
    cli
    push byte 8
    jmp isr_common_stub

; 9: Coprocessor Segment Overrun Exception
isr9:
    cli
    push byte 0
    push byte 9
    jmp isr_common_stub

; 10: Bad TSS Exception (With Error Code!)
isr10:
    cli
    push byte 10
    jmp isr_common_stub

; 11: Segment Not Present Exception (With Error Code!)
isr11:
    cli
    push byte 11
    jmp isr_common_stub

; 12: Stack Fault Exception (With Error Code!)
isr12:
    cli
    push byte 12
    jmp isr_common_stub

; 13: General Protection Fault Exception (With Error Code!)
isr13:
    cli
    push byte 13
    jmp isr_common_stub

; 14: Page Fault Exception (With Error Code!)
isr14:
    cli
    push byte 14
    jmp isr_common_stub

; 15: Reserved Exception
isr15:
    cli
    push byte 0
    push byte 15
    jmp isr_common_stub

; 16: Floating Point Exception
isr16:
    cli
    push byte 0
    push byte 16
    jmp isr_common_stub

; 17: Alignment Check Exception
isr17:
    cli
    push byte 0
    push byte 17
    jmp isr_common_stub

; 18: Machine Check Exception
isr18:
    cli
    push byte 0
    push byte 18
    jmp isr_common_stub

; 19: Reserved
isr19:
    cli
    push byte 0
    push byte 19
    jmp isr_common_stub

; 20: Reserved
isr20:
    cli
    push byte 0
    push byte 20
    jmp isr_common_stub

; 21: Reserved
isr21:
    cli
    push byte 0
    push byte 21
    jmp isr_common_stub

; 22: Reserved
isr22:
    cli
    push byte 0
    push byte 22
    jmp isr_common_stub

; 23: Reserved
isr23:
    cli
    push byte 0
    push byte 23
    jmp isr_common_stub

; 24: Reserved
isr24:
    cli
    push byte 0
    push byte 24
    jmp isr_common_stub

; 25: Reserved
isr25:
    cli
    push byte 0
    push byte 25
    jmp isr_common_stub

; 26: Reserved
isr26:
    cli
    push byte 0
    push byte 26
    jmp isr_common_stub

; 27: Reserved
isr27:
    cli
    push byte 0
    push byte 27
    jmp isr_common_stub

; 28: Reserved
isr28:
    cli
    push byte 0
    push byte 28
    jmp isr_common_stub

; 29: Reserved
isr29:
    cli
    push byte 0
    push byte 29
    jmp isr_common_stub

; 30: Reserved
isr30:
    cli
    push byte 0
    push byte 30
    jmp isr_common_stub

; 31: Reserved
isr31:
    cli
    push byte 0
    push byte 31
    jmp isr_common_stub

; IRQ handlers
irq0:
    cli
    push byte 0
    push byte 32
    jmp irq_common_stub

irq1:
    cli
    push byte 1
    push byte 33
    jmp irq_common_stub

irq2:
    cli
    push byte 2
    push byte 34
    jmp irq_common_stub

irq3:
    cli
    push byte 3
    push byte 35
    jmp irq_common_stub

irq4:
    cli
    push byte 4
    push byte 36
    jmp irq_common_stub

irq5:
    cli
    push byte 5
    push byte 37
    jmp irq_common_stub

irq6:
    cli
    push byte 6
    push byte 38
    jmp irq_common_stub

irq7:
    cli
    push byte 7
    push byte 39
    jmp irq_common_stub

irq8:
    cli
    push byte 8
    push byte 40
    jmp irq_common_stub

irq9:
    cli
    push byte 9
    push byte 41
    jmp irq_common_stub

irq10:
    cli
    push byte 10
    push byte 42
    jmp irq_common_stub

irq11:
    cli
    push byte 11
    push byte 43
    jmp irq_common_stub

irq12:
    cli
    push byte 12
    push byte 44
    jmp irq_common_stub

irq13:
    cli
    push byte 13
    push byte 45
    jmp irq_common_stub

irq14:
    cli
    push byte 14
    push byte 46
    jmp irq_common_stub

irq15:
    cli
    push byte 15
    push byte 47
    jmp irq_common_stub

sys:
    cli
    push byte 0
    push byte 0
    jmp syscall_handler
