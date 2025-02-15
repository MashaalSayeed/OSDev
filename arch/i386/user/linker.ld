ENTRY(_start)

SECTIONS {
    . = 0x400000; /* User-space programs typically start at a higher address */

    .text : {
        *(.text)
    }

    .rodata : {
        *(.rodata)
    }

    .data : {
        *(.data)
    }

    .bss : {
        *(.bss)
    }
}
