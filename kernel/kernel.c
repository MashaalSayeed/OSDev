#include "kernel.h"
#include "../cpu/isr.h"
#include "../drivers/screen.h"
#include "../libc/string.h"
#include "../libc/mem.h"

void kernel_main() {
    isr_install();
    irq_install();

    clear_screen();
    division_by_zero();

    print_string("Type something, it will go through the kernel\n"
        "Type END to halt the CPU\n> ");
}

void division_by_zero() {
    int x = 1;
    int y = 0;
    x = x / y;
}

void user_input(char *input) {
    if (strcmp(input, "END") == 0) {
        print_string("Stopping the CPU. Bye!\n");
        asm volatile("hlt");
    } else if (strcmp(input, "PAGE") == 0) {
        uint32_t phys_addr;
        uint32_t page = kmalloc(1000, 1, &phys_addr);
        char page_str[16] = "";
        hex_to_ascii(page, page_str);
        char phys_str[16] = "";
        hex_to_ascii(phys_addr, phys_str);
        print_string("Page: ");
        print_string(page_str);
        print_string(", physical address: ");
        print_string(phys_str);
        print_string("\n");
    }

    print_string("You said: ");
    print_string(input);
    print_string("\n> ");
}