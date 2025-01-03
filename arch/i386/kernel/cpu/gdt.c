#include "kernel/gdt.h"
#include "libc/string.h"

extern void gdt_flush(void* addr);
extern void tss_flush();

// Define the GDT
struct gdt_entry gdt[GDT_ENTRY_COUNT];
struct gdt_ptr gp;
struct tss_entry tss_entry;

void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[num].base_low = (base & 0xFFFF);
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high = (base >> 24) & 0xFF;

    gdt[num].limit_low = (limit & 0xFFFF);
    gdt[num].granularity = (limit >> 16) & 0x0F;

    gdt[num].granularity |= gran & 0xF0;
    gdt[num].access = access;
}

void load_tss(uint32_t num, uint16_t ss0, uint32_t esp0) {
    uint32_t base = (uint32_t)&tss_entry;
    uint32_t limit = base + sizeof(struct tss_entry);

    gdt_set_gate(num, base, limit, 0xE9, 0x00);

    memset(&tss_entry, 0, sizeof(struct tss_entry));

    tss_entry.esp0 = esp0;
    tss_entry.ss0 = ss0;
    tss_entry.cs = 0x08 | 0x03;
    tss_entry.ss = tss_entry.ds = tss_entry.es = tss_entry.fs = tss_entry.gs = 0x10 | 0x03;
    tss_entry.trap = 0x8;

    // asm volatile ("ltr %0" : : "r"(num << 3));
}

void gdt_install() {
    gp.limit = (sizeof(struct gdt_entry) * GDT_ENTRY_COUNT) - 1;
    gp.base = (uint32_t)&gdt;

    gdt_set_gate(0, 0, 0, 0, 0);                 // Null segment
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);  // Kernel Code segment 0x08
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);  // Kernel Data segment 0x10
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);  // User Code segment   0x1B
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);  // User Data segment   0x23
    load_tss(5, 0x10, 0x0);

    gdt_flush(&gp);
    tss_flush();

    asm volatile ("lgdt %0" : : "m"(gp));
    asm volatile ("mov $0x10, %ax; mov %ax, %ds; mov %ax, %es; mov %ax, %fs; mov %ax, %gs; ljmp $0x08, $next; next:");
}