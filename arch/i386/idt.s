.globl idt_flush
.extern isr_handler
.extern irq_handler

# Flush the IDT (load the IDT descriptor)
idt_flush:
    movl 4(%esp), %eax          # Load argument from the stack (IDT descriptor)
    lidt (%eax)                 # Load IDT descriptor
    ret                         # Return from function

# Common ISR handler stub
# This is used for both error and non-error ISRs
isr_common_stub:
    pushal                      # Push all general purpose registers
    pushl %ds                   # Push data segment register
    pushl %esp                  # Push current stack pointer (for stack tracing)

    cld
    call isr_handler            # Call the common ISR handler (defined in C)

    pop %eax
    popl %ds
    popal                       # Restore general purpose registers
    addl $8, %esp               # Adjust stack (remove pushed %esp)
    sti                         # Re-enable interrupts
    iret                        # Return from interrupt

irq_common_stub:
    pushal                      # Push all general-purpose registers
    mov %ds, %ax
    pushl %eax
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    pushl %esp                  # Push current stack pointer (for stack tracing)

    cld
    call irq_handler            # Call the common ISR handler (defined in C)

    addl $4, %esp               # Adjust stack (remove pushed %esp)
    popl %ebx

    mov %bx, %ds
    mov %bx, %es
    mov %bx, %fs
    mov %bx, %gs
    popal                       # Restore general-purpose registers

    addl $8, %esp               # Adjust stack (remove pushed int_no & error_code)
    sti
    iret                        # Return from interrupt

# Macro for defining ISRs without error codes
.macro isr_no_error num
.section .text
.global isr\num
isr\num:
    cli                         # Disable interrupts
    pushl $0                    # Push a dummy error code (0) for non-error ISRs
    pushl $\num                 # Push the ISR number
    jmp isr_common_stub          # Jump to the common ISR handler stub
.endm

# Macro for defining ISRs with error codes
.macro isr_error num
.section .text
.global isr\num
isr\num:
    cli                         # Disable interrupts
    pushl $\num                 # Push the ISR number
    jmp isr_common_stub          # Jump to the common ISR handler stub (error code is already pushed by the CPU)
.endm

.macro irq_macro num irq_num
.section .text
.global irq\num
irq\num:
    cli                          # Disable interrupts
    pushl $\num                     # Push a dummy error code (0) for IRQs
    pushl $\irq_num              # Push the IRQ number
    jmp irq_common_stub          # Jump to the common ISR handler stub
.endm


# Define the ISRs (0-31)
# ISRs with error codes (exceptions 8, 10-14)
isr_error 8
isr_error 10
isr_error 11
isr_error 12
isr_error 13
isr_error 14

# ISRs without error codes (exceptions 0-7, 9, 15-31)
isr_no_error 0
isr_no_error 1
isr_no_error 2
isr_no_error 3
isr_no_error 4
isr_no_error 5
isr_no_error 6
isr_no_error 7
isr_no_error 9
isr_no_error 15
isr_no_error 16
isr_no_error 17
isr_no_error 18
isr_no_error 19
isr_no_error 20
isr_no_error 21
isr_no_error 22
isr_no_error 23
isr_no_error 24
isr_no_error 25
isr_no_error 26
isr_no_error 27
isr_no_error 28
isr_no_error 29
isr_no_error 30
isr_no_error 31

isr_no_error 128
isr_no_error 177

irq_macro 0, 32
irq_macro 1, 33
irq_macro 2, 34
irq_macro 3, 35
irq_macro 4, 36
irq_macro 5, 37
irq_macro 6, 38
irq_macro 7, 39
irq_macro 8, 40
irq_macro 9, 41
irq_macro 10, 42
irq_macro 11, 43
irq_macro 12, 44
irq_macro 13, 45
irq_macro 14, 46
irq_macro 15, 47

