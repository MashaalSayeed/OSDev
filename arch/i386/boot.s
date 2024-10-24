; Constants for the Multiboot header
MBOOT_PAGE_ALIGN EQU 1 << 0
MBOOT_MEM_INFO EQU 1 << 1
MBOOT_USE_GFX EQU 0

MBOOT_MAGIC EQU 0x1BADB002
MBOOT_FLAGS EQU MBOOT_PAGE_ALIGN | MBOOT_MEM_INFO | MBOOT_USE_GFX
MBOOT_CHECKSUM EQU -(MBOOT_MAGIC + MBOOT_FLAGS)

; Constants for loading higher half kernel
VM_BASE     equ 0xC0000000
PDE_INDEX   equ (VM_BASE >> 22)


section .multiboot
ALIGN 4
    DD MBOOT_MAGIC
    DD MBOOT_FLAGS
    DD MBOOT_CHECKSUM
    DD 0, 0, 0, 0, 0

    DD 0
    DD 800
    DD 600
    DD 32


; Page Directory: Contains 1024 entries each pointing to a page table
; Page Table: Contains 1024 entries each pointing to a page
; Page: Contains 4KB of memory
section .data
align 4096
global initial_page_dir
initial_page_dir:
    DD 10000011b                      ; B0 - Present | B1 - Read/Write | B7 - Page Size
    TIMES (PDE_INDEX-1) DD 0          ; Map first 4MB to identity map

    DD (0 << 22) | 10000011b          ; Create PDEs for higher half kernel
    DD (1 << 22) | 10000011b
    DD (2 << 22) | 10000011b
    DD (3 << 22) | 10000011b
    TIMES (1024 - PDE_INDEX - 1) DD 0 ; Zero out the rest of the PDEs (Unused)


SECTION .bss
ALIGN 16
stack_bottom:
    RESB 16384 * 8
stack_top:

section .boot

global _start
_start:
    ; Update the page directory address
    MOV ecx, (initial_page_dir - 0xC0000000)
    MOV cr3, ecx

    ; Enable 4MB pages
    MOV ecx, cr4
    OR ecx, 0x10
    MOV cr4, ecx

    ; Enable paging
    MOV ecx, cr0
    OR ecx, 0x80000000
    MOV cr0, ecx

    LEA ecx, [higher_half]
    JMP ecx

section .text
higher_half:
    ; Set up the stack
    MOV esp, stack_top

    ; Pass the Multiboot information to the kernel
    PUSH ebx
    PUSH eax

    XOR ebp, ebp ; Clear the frame pointer to terminate the stack trace

    extern kernel_main
    CALL kernel_main

halt:
    hlt
    JMP halt
