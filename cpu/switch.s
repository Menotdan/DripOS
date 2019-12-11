.section .text
.global switchTask
.extern oof
.extern eax
.extern eip
.extern esp

switchTask:
    #mov 4(%esp), %eax # Move the address of the struct to eax
    #pop %ebx # Clean up after caller
    mov oof, %eax
    mov 4(%eax), %ebx # EBX
    mov 8(%eax), %ecx # ECX
    mov 12(%eax), %edx # EDX
    mov 16(%eax), %esi # ESI
    mov 20(%eax), %edi # EDI
    mov 24(%eax), %esp # ESP
    mov %esp, esp
    push %eax # Save eax
    mov 32(%eax), %eax # Move EIP to EAX
    # Move the new EIP value into esp, and eax is returned to its previous state
    mov (%esp), %ebp
    mov %eax, (%esp)
    mov %ebp, %eax
    mov 36(%eax), %ebp # EFLAGS
    push %ebp # Push the flags(stored in ebp)
    popf # Pop the value we just pushed into the flags register, restores interrupt flag, because each process is started with the interrupt flag on, and disabling it would kill the task system (secret info dont tell processes)
    mov 28(%eax), %ebp # EBP
    mov (%eax), %eax # Set EAX
    sti
    ret # Return to the EIP in stack