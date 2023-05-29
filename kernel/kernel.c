// #include "../drivers/screen.h"
#include "../cpu/idt.h"
#include "../cpu/isr.h"

void main() {
    isr_install();

	// clear_screen();
    /* Test the interrupts */
    __asm__ __volatile__("int $2");
    // __asm__ __volatile__("int $3");
}
