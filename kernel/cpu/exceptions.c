#include "kernel/isr.h"
#include "kernel/exceptions.h"
#include "kernel/printf.h"
#include "kernel/elf.h"
#include "kernel/paging.h"
#include "kernel/process.h"
#include "kernel/system.h"
#include "kernel/pmm.h"

extern elf_t kernel_elf;
extern uint8_t kernel_stack_bottom;
extern uint32_t kpage_dir_phys;

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
    printf("  USERESP: %x\n", regs->useresp);
    printf("  SS: %x\n", regs->ss);
    printf("  EAX: %x\n", regs->eax);
}

void print_stack_trace(registers_t *regs) {
    uint32_t *ebp, *eip;
    uint32_t count = 0;

    if (regs) {
        ebp = (uint32_t *)regs->ebp;
    } else {
        asm volatile("mov %%ebp, %0" : "=r" (ebp));
    }

    printf("\nStack Trace:\n");
    while (ebp) {
        if (count > 15) return;

        char *symbol = elf_lookup_symbol(*eip, &kernel_elf);
        if (!symbol) symbol = "N/A";
        
        eip = ebp + 1;
        printf(" %d %x <%s>\n", count, *eip, symbol);
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
    print_stack_trace(regs);
    panic();
}

static void double_fault_handler(registers_t *regs) {
    printf("\nError: Double Fault\n");
    print_debug_info(regs);
    panic();
}

static void triple_fault_handler(registers_t *regs) {
    printf("\nError: Triple Fault\n");
    print_debug_info(regs);
    print_stack_trace(regs);
    panic();
}

int page_fault_detected = 0;
void page_fault_handler(registers_t *regs) {
    if (!is_paging_enabled()) {
        printf("Page fault occurred but paging is not enabled!\n");
        for (;;) ;
    }

    if (page_fault_detected) {
        printf("Nested page fault detected! Halting system.\n");
        for (;;) ;
    }

    page_fault_detected = 1;

    uint32_t faulting_address, cr3;
    asm volatile("mov %%cr2, %0" : "=r" (faulting_address) :: "memory");
    get_cr3(&cr3);
    
    printf("Page fault at %x\n", faulting_address);
    printf("Error code: %x\n", regs->err_code);
    printf("Page Directory: %x (kernel: %s)\n", cr3, cr3 == kpage_dir_phys ? "yes" : "no");
    print_debug_info(regs);

    if (faulting_address >= &kernel_stack_bottom && faulting_address < &kernel_stack_bottom + BLOCK_SIZE * 4) {
        printf("Stack overflow at %x\n", faulting_address);
        for (;;) ;
    }
    
    // kprintf(DEBUG, "page_fault: eip=%x esp=%x cr2=%x err=%x cs=%x\n",
    //         regs->eip, regs->useresp, faulting_address, regs->err_code, regs->cs);
    
    process_t *proc = get_current_process();
    if (proc) printf("PID: %d\n", proc->pid);

    uint32_t present = regs->err_code & PF_ERR_PRESENT;
    uint32_t rw = regs->err_code & PF_ERR_RW;
    uint32_t user = regs->err_code & PF_ERR_USER;
    uint32_t reserved = regs->err_code & PF_ERR_RESERVED;
    uint32_t inst_fetch = regs->err_code & PF_ERR_INST;

    printf("Possible causes: [ ");
    if(!present) printf("Page not present ");
    if(rw) printf("Page is read only ");
    if(user) printf("User-mode access ");
    if(reserved) printf("Overwrote reserved bits ");
    if(inst_fetch) printf("Instruction fetch ");
    printf("]\n");

    print_stack_trace(regs);

    if (user && proc) {
        printf("Killing process %d due to page fault\n", proc->pid);
        page_fault_detected = 0;   // reset before kill_process calls schedule
        kill_process(proc, -1);
        // unreachable if schedule() switches away
    }

    page_fault_detected = 0;  // reset so nested detection still works
    for (;;);
}

void exceptions_install() {
    register_interrupt_handler(0, &divide_by_zero_handler);
    register_interrupt_handler(8, &double_fault_handler);
    register_interrupt_handler(13, &gpf_handler);
    register_interrupt_handler(30, &triple_fault_handler); // Triple fault handler
    register_interrupt_handler(14, &page_fault_handler);
}