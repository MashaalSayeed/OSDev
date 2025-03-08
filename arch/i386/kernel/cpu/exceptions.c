#include "kernel/isr.h"
#include "kernel/exceptions.h"
#include "kernel/printf.h"

// TODO: make implementation of all exception handlers
static void print_debug_info(registers_t *regs) {
    printf("  EIP: %x\n", regs->eip);
    printf("  CS: %x\n", regs->cs);
    printf("  EFLAGS: %x\n", regs->eflags);
    printf("  ESP: %x\n", regs->esp);
    printf("  SS: %x\n", regs->ss);
}

// void print_stack_trace() {
//     uint32_t *ebp;
//     asm volatile("mov %%ebp, %0" : "=r" (ebp));

//     printf("Stack Trace:\n");
//     while (ebp) {
//         uint32_t eip = ebp[1];
//         printf("  [%x]\n", eip);

//         ebp = (uint32_t *)ebp[0];  // Move to previous frame
//     }
// }

static void divide_by_zero_handler(registers_t *regs) {
    printf("Error: Divide by Zero Exception\n");
    print_debug_info(regs);
    
    while (1) asm volatile("hlt");
}

static void gpf_handler(registers_t *regs) {
    printf("Error: General Protection Fault (GPF)\n");
    printf("  Error Code: %x\n", regs->err_code);

    // Decode error code
    if (regs->err_code & 1) printf("  Caused by an external event\n");
    if ((regs->err_code & 2) == 0) printf("  Fault occurred in GDT\n");
    else if ((regs->err_code & 2) == 1) printf("  Fault occurred in IDT\n");
    else printf("  Fault occurred in LDT\n");
    
    printf("  Selector index: %d\n", regs->err_code >> 3);

    // Print CPU state at time of exception
    print_debug_info(regs);

    // Halt system
    while (1) asm volatile("hlt");
}

static void double_fault_handler(registers_t *regs) {
    printf("Error: Double Fault\n");
    print_debug_info(regs);

    while (1) asm volatile("hlt");
}

void exceptions_install() {
    register_interrupt_handler(0, &divide_by_zero_handler);
    register_interrupt_handler(8, &double_fault_handler);
    register_interrupt_handler(13, &gpf_handler);
}