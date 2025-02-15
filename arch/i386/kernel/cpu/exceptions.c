#include "kernel/isr.h"
#include "kernel/exceptions.h"
#include "kernel/printf.h"

// TODO: make implementation of all exception handlers
void gpf_handler(registers_t *regs) {
    printf("General Protection Fault (GPF)\n");
    printf("  Error Code: %x\n", regs->err_code);

    // Decode error code
    if (regs->err_code & 1) printf("  Caused by an external event\n");
    if ((regs->err_code & 2) == 0) printf("  Fault occurred in GDT\n");
    else if ((regs->err_code & 2) == 1) printf("  Fault occurred in IDT\n");
    else printf("  Fault occurred in LDT\n");
    
    printf("  Selector index: %d\n", regs->err_code >> 3);

    // Print CPU state at time of exception
    printf("  Registers:\n");
    printf("    EIP:  %x (Faulting instruction)\n", regs->eip);
    printf("    CS:   %x\n", regs->cs);
    printf("    EFLAGS: %x\n", regs->eflags);
    printf("    ESP:  %x\n", regs->esp);
    printf("    SS:   %x\n", regs->ss);

    // Halt system
    while (1) asm volatile("hlt");
}

void exceptions_install() {
    register_interrupt_handler(13, &gpf_handler);
}