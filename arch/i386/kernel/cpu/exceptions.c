#include "kernel/isr.h"
#include "kernel/exceptions.h"
#include "kernel/printf.h"
#include "kernel/elf.h"

extern elf_t kernel_elf;

static void panic() {
    asm volatile("cli");
    while (1) asm volatile("hlt");
}

// TODO: make implementation of all exception handlers
void print_debug_info(registers_t *regs) {
    char * symbol = elf_lookup_symbol(regs->eip, &kernel_elf);
    if (!symbol) symbol = "N/A";
    printf("  EIP: %x <%s>\n", regs->eip, symbol);
    printf("  CS: %x\n", regs->cs);
    printf("  EFLAGS: %x\n", regs->eflags);
    printf("  ESP: %x\n", regs->esp);
    printf("  SS: %x\n", regs->ss);
}

void print_stack_trace(registers_t *regs) {
    uint32_t *ebp, *eip;
    uint32_t count = 0;
    ebp = (uint32_t *)regs->ebp;

    printf("\nStack Trace:\n");
    while (ebp) {
        if (count > 15) return;
        
        eip = ebp + 1;
        printf(" %d %x <%s>\n", count, *eip, elf_lookup_symbol(*eip, &kernel_elf));
        ebp = (uint32_t *)ebp[0];  // Move to previous frame
        count++;
    }
}

static void divide_by_zero_handler(registers_t *regs) {
    printf("\nError: Divide by Zero Exception\n");
    print_debug_info(regs);
    print_stack_trace(regs);
    panic();
}

static void gpf_handler(registers_t *regs) {
    printf("\nError: General Protection Fault (GPF)\n");
    printf("  Error Code: %x\n", regs->err_code);

    // Decode error code
    if (regs->err_code & 1) printf("  Caused by an external event\n");
    if ((regs->err_code & 2) == 0) printf("  Fault occurred in GDT\n");
    else if ((regs->err_code & 2) == 1) printf("  Fault occurred in IDT\n");
    else printf("  Fault occurred in LDT\n");
    
    printf("  Selector index: %d\n", regs->err_code >> 3);

    // Print CPU state at time of exception
    print_debug_info(regs);
    panic();
}

static void double_fault_handler(registers_t *regs) {
    printf("\nError: Double Fault\n");
    print_debug_info(regs);
    panic();
}

void exceptions_install() {
    register_interrupt_handler(0, &divide_by_zero_handler);
    register_interrupt_handler(8, &double_fault_handler);
    register_interrupt_handler(13, &gpf_handler);
}