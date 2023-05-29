[org 0x7c00]
KERNEL_OFFSET equ 0x1000 ; Memory offset where we load the kernel

    mov [BOOT_DRIVE], dl  ; Remember that the BIOS sets us the boot drive in 'dl' on boot

    mov bp, 0x9000
    mov sp, bp
    
    mov bx, MSG_REAL_MODE
    call print_string
    call print_nl
    
    call load_kernel
    
    call switch_to_pm ; We never return from here

    jmp $

%include "boot/bios/print.asm"
%include "boot/bios/print-hex.asm"
%include "boot/bios/disk.asm"
%include "boot/pm/gdt.asm"
%include "boot/pm/switch.asm"
%include "boot/pm/print.asm"

[bits 16]
load_kernel:
    mov bx, MSG_LOAD_KERNEL
    call print_string
    call print_nl

    ; mov dx, [BOOT_DRIVE]
    ; call print_hex
    ; call print_nl

    mov bx, KERNEL_OFFSET
    mov dh, 32 ; Load first 15 sectors from boot disk to kernel offset
    mov dl, [BOOT_DRIVE]
    call disk_load

    ret


[bits 32]
BEGIN_PM:
    mov ebx, MSG_PROT_MODE
    call print_string_pm ; Use 32 bit print

    call KERNEL_OFFSET ; Jump to kernel code!

    jmp $ ; Hang

; Constants
BOOT_DRIVE db 0
MSG_LOAD_KERNEL db "Loading kernel into memory", 0
MSG_REAL_MODE db "Started in 16-bit Real Mode!", 0
MSG_PROT_MODE db "Switched to 32-bit Protected Mode!", 0

; Bootsector Padding
times 510 - ($-$$) db 0
dw 0xaa55
