.globl gdt_flush

gdt_flush:
    # Load the GDT pointer (argument passed to the function)
    movl 4(%esp), %eax          # Load argument from the stack (GDT descriptor)
    lgdt (%eax)                 # Load GDT descriptor

    # Load the segment selectors
    movl $0x10, %eax            # Data segment selector (0x10)
    movw %ax, %ds               # Load DS
    movw %ax, %es               # Load ES
    movw %ax, %fs               # Load FS
    movw %ax, %gs               # Load GS
    movw %ax, %ss               # Load SS

    # Jump to the new code segment
    ljmp $0x08, $.flush         # Far jump to code segment (0x08)

.flush:
    ret                         # Return to the caller
