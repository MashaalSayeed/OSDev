#ifndef IDT_H
#define IDT_H

#include <stdint.h>

#define KERNEL_CS 0x08

/* How every interrupt gate (handler) is defined */
typedef struct {
	uint16_t    isr_low;      /* The lower 16 bits of the ISR's address */
	uint16_t    kernel_cs;    /* The GDT segment selector that the CPU will load into CS before calling the ISR */
	uint8_t     reserved;     /* Set to zero */
	uint8_t     attributes;   /* Type and attributes */
	uint16_t     isr_high;     /* The higher 16 bits of the ISR's address */
} __attribute__((packed)) idt_entry_t;

/* A pointer to the array of interrupt handlers. Assembly instruction 'lidt' will read it */
typedef struct {
	uint16_t	limit;
	uint32_t	base;
} __attribute__((packed)) idt_register_t;


void set_idt_entry(int n, uint32_t isr);
void set_idt();

#endif