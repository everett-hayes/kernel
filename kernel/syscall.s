.global syscall

syscall:
    # Copy the 7th param into %rax (the return register)
    mov 0x8(%rsp), %rax

    # issue syscall
    int $0x80

    # return 
    retq

