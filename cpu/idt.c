#include "idt.h"

#define IDT_ENTRIES 256
idt_entry_t idt[IDT_ENTRIES];
idt_register_t idtr;

void set_idt_entry(int n, uint32_t isr) {
    idt[n].isr_low = isr & 0xFFFF;
    idt[n].kernel_cs = KERNEL_CS;
    idt[n].reserved = 0;
    idt[n].attributes = 0x8E; 
    idt[n].isr_high = (isr >> 16) & 0xFFFF;
}

void set_idt() {
    idtr.base = (uint32_t) &idt;
    idtr.limit = IDT_ENTRIES * sizeof(idt_entry_t) - 1;
    /* Don't make the mistake of loading &idt -- always load &idt_reg */
    __asm__ __volatile__("lidtl (%0)" : : "r" (&idtr));
}