.section .text
.global switchTask
#switchTask:
#    cli
#    pusha
#    pushf
#    mov %cr3, %eax #Push CR3
#    push %eax
#    mov 44(%esp), %eax #The first argument, where to save
#    mov %ebx, 4(%eax)
#    mov %ecx, 8(%eax)
#    mov %edx, 12(%eax)
#    mov %esi, 16(%eax)
#    mov %edi, 20(%eax)
#    mov 36(%esp), %ebx #EAX
#    mov 40(%esp), %ecx #IP
#    mov 20(%esp), %edx #ESP
#    add $4, %edx #Remove the return value ;)
#    mov 16(%esp), %esi #EBP
#    mov 4(%esp), %edi #EFLAGS
#    mov %ebx, (%eax)
#    mov %edx, 24(%eax)
#    mov %esi, 28(%eax)
#    mov %ecx, 32(%eax)
#    mov %edi, 36(%eax)
#    pop %ebx #CR3
#    mov %ebx, 40(%eax)
#    push %ebx #Goodbye again ;)
#    mov 48(%esp), %eax #Now it is the new object
#    mov 4(%eax), %ebx #EBX
#    mov 8(%eax), %ecx #ECX
#    mov 12(%eax), %edx #EDX
#    mov 16(%eax), %esi #ESI
#    mov 20(%eax), %edi #EDI
#    mov 28(%eax), %ebp #EBP
#    push %eax
#    mov 36(%eax), %eax #EFLAGS
#    push %eax
#    popf
#    pop %eax
#    mov 24(%eax), %esp #ESP
#    push %eax
#    mov 40(%eax), %eax #CR3
#    mov %eax, %cr3
#    pop %eax
#    push %eax
#    mov 32(%eax), %eax #EIP
#    xchg (%esp), %eax #We do not have any more registers to use as tmp storage
#    mov (%eax), %eax #EAX
#    sti
#    ret #This ends all!

switchTask:
    #cli Clear interrupt flag, to prevent an interrupt from firing while task is switching
    mov 4(%esp), %eax # Move the top of the struct to eax
    mov 4(%eax), %ebx # EBX
    mov 8(%eax), %ecx # ECX
    mov 12(%eax), %edx # EDX
    mov 16(%eax), %esi # ESI
    mov 20(%eax), %edi # EDI
    mov 24(%eax), %esp # ESP
    push %eax # Save eax
    mov 32(%eax), %eax # Move EIP to EAX
    xchg (%esp), %eax # Move the new EIP value into esp, and eax is returned to its previous state
    mov 36(%eax), %ebp # EFLAGS
    push %ebp # Push the flags(stored in ebp)
    popf # Pop the value we just pushed into the flags register, restores interrupt flag, because each process is started with the interrupt flag on, and disabling it would kill the task system (secret info dont tell processes)
    mov 28(%eax), %ebp # EBP
    mov (%eax), %eax # Put EAX back
    push %eax
    mov $102, %ax
    mov %ax, (0xb8000) # put f at screen
    mov %ax, (0xb8002) # put f at screen
    mov %ax, (0xb8004) # put f at screen
    pop %eax
    sti
    ret # Return to the EIP in stack