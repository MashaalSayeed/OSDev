/* Link.ld -- Linker script for the kernel - ensure everything goes in the
   Correct place.  
   Original file taken from Bran's Kernel Development tutorials: 
   http://www.osdever.net/bkerndev/index.php.
*/

ENTRY(_start)

SECTIONS
{
    . = 0x10000;  /* Starting address of the Multiboot header */
    _kernel_start = .;

    /* Multiboot2 Header Section */
    .multiboot ALIGN(4K): {
        *(.multiboot)   /* Include the multiboot section here */
    }

    /* .note.gnu.build-id Section (optional, for debugging) */
    .note.gnu.build-id ALIGN(4K): {
        *(.note.gnu.build-id)
    }

    /* Boot Section (if you have any bootloader code, like for GRUB) */
    .boot ALIGN(4K): {
        *(.boot)
    }

    /* Now we jump to the higher-half kernel address space (0xC0000000) */
    . += 0xC0000000;   /* Set the current address after the multiboot header */

    /* Kernel Sections: All kernel sections must be mapped in the higher half */
    .text ALIGN(4K): AT(ADDR(.text) - 0xC0000000) {
        *(.text)   /* Kernel code section */
    }

    .rodata ALIGN(4K): AT(ADDR(.rodata) - 0xC0000000) {
        *(.rodata)  /* Read-only data */
    }

    .data ALIGN(4K): AT(ADDR(.data) - 0xC0000000) {
        *(.data)    /* Initialized data */
    }

    .bss ALIGN(4K): AT(ADDR(.bss) - 0xC0000000) {
        *(COMMON)   /* Common symbols (uninitialized) */
        *(.bss)     /* BSS section (uninitialized data) */
    }

    _kernel_end = .;   /* End of the kernel */
}
