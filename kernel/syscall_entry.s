.global syscall_entry
.global syscall_handler

syscall_entry:
    # put the 7th param on the stack
    push %rax

    # call the C-land syscall handler
    call syscall_handler

    # remove the stack head
    add $0x8, %rsp

    # return from the int handler
    iretq