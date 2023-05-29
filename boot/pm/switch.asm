[bits 16]
switch_to_pm:
    cli ; disable interrupts
    lgdt [gdt_descriptor] ; load gdt_descriptor

    mov eax, cr0
    or eax, 0x1 ; set the 32 bit mode bit in control register cr0
    mov cr0, eax

    jmp CODE_SEG:init_pm ; far jump to different segment - clears cpu instruction queue

[bits 32]
; Assembler directive to now use 32 bit instructions
init_pm:
    mov ax, DATA_SEG ; update all segment registers
    mov ds, ax
    mov ss, ax
    mov es, ax 
    mov fs, ax
    mov gs, ax

    mov ebp, 0x90000 ; initialise stack
    mov esp , ebp

    call BEGIN_PM ; call well known label
