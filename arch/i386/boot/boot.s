; Constants for Multiboot2 header and tags
MBOOT_MAGIC        EQU 0xE85250D6
MBOOT_ARCH         EQU 0
MBOOT_HEADER_LENGTH EQU (multiboot_header_end - multiboot_header)
MBOOT_HEADER_CHECKSUM EQU -(MBOOT_MAGIC + MBOOT_ARCH + MBOOT_HEADER_LENGTH)

; Multiboot2 Tag Types
HEADER_TAG_END     EQU 0
HEADER_TAG_OPTIONAL EQU 1
HEADER_TAG_CMDLINE EQU 2
HEADER_TAG_MODULES EQU 3
HEADER_TAG_FRAMEBUFFER EQU 5

; Constants for loading higher half kernel
VM_BASE            EQU 0xC0000000
PDE_INDEX          EQU (VM_BASE >> 22)

; Multiboot Header Section
section .multiboot align=8
global multiboot_header
multiboot_header:
    DD MBOOT_MAGIC             ; Multiboot2 magic number
    DD MBOOT_ARCH              ; Architecture (0 for i686)
    DD MBOOT_HEADER_LENGTH     ; Length of header in bytes
    DD MBOOT_HEADER_CHECKSUM   ; Header checksum

%ifdef ENABLE_GUI
mb2_tag_framebuffer_start:
    DW HEADER_TAG_FRAMEBUFFER
    DW 0
    DD mb2_tag_framebuffer_end - mb2_tag_framebuffer_start
    DD 1024
    DD 768
    DD 32
mb2_tag_framebuffer_end:
%endif

    align 8
    dw 6                        ; Tag type 6 (module alignment)
    dw 0                        ; Flags (0)
    dd 8                        ; Size of this tag (8 bytes)

    ; End tag
    align 8
mb2_tag_end_start:
    DW HEADER_TAG_END          ; End tag
    DW 0                       ; Flags (reserved, must be 0)
    DD mb2_tag_end_end - mb2_tag_end_start ; End tag size in bytes (8 bytes for end tag)
mb2_tag_end_end:
multiboot_header_end:

; Page Directory Section
section .data
align 4096
global initial_page_dir
initial_page_dir:
    DD 10000011b                  ; Present, Read/Write, Page Size bit set for 4MB pages
    TIMES (PDE_INDEX-1) DD 0       ; Identity map first 4MB (null entries)
    DD (0 << 22) | 10000011b       ; PDE for higher half kernel
    DD (1 << 22) | 10000011b       ; More PDEs for higher half kernel
    DD (2 << 22) | 10000011b
    DD (3 << 22) | 10000011b
    TIMES (1024 - PDE_INDEX - 1) DD 0 ; Zero out unused PDEs

; Stack Section
section .bss
align 16
global stack_top
global stack_bottom
stack_bottom:
    RESB 16384 * 8  ; Reserve 16KB stack
stack_top:

; Boot Section (Entry Point)
section .boot
global _start
_start:
    ; Update the page directory address
    MOV ecx, (initial_page_dir - 0xC0000000)  ; Update for higher-half kernel
    MOV cr3, ecx    ; Load new page directory address into CR3

    ; Enable 4MB pages (set PSE flag in CR4)
    MOV ecx, cr4
    OR ecx, 0x10    ; Set PSE (Page Size Extension) bit
    MOV cr4, ecx

    ; Enable paging (set PG flag in CR0)
    MOV ecx, cr0
    OR ecx, 0x80000000  ; Set PG bit (paging enabled)
    MOV cr0, ecx

    ; Jump to the higher half kernel
    LEA ecx, [higher_half]
    JMP ecx

section .text
higher_half:
    ; Set up the stack pointer to the top of the stack
    MOV esp, stack_top

    ; Pass Multiboot information to the kernel (optional)
    PUSH ebx
    PUSH eax

    XOR ebp, ebp       ; Clear ebp register for stack trace termination

    extern kernel_main
    CALL kernel_main   ; Call the kernel's entry point

halt:
    HLT                ; Halt the system (in case of failure)
    JMP halt           ; Infinite loop


global jump_usermode
jump_usermode:
    mov ebx, [esp+8]  ; Get the address of the user function

    mov ax, (4 * 8) | 3  ; User data segment selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; mov eax, esp

    ; Set up the stack frame for iret
    push (4 * 8) | 3      ; User data segment selector
    push esp              ; User space stack pointer
    pushf                 ; Push EFLAGS
    push (3 * 8) | 3      ; User code segment selector
    push ebx              ; Address of the user function
    iret