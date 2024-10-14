.set ALIGN,    1<<0             # align loaded modules on page boundaries
.set MEMINFO,  1<<1             # provide memory map
.set GFXINFO,  0             # provide video information

.set FLAGS,    ALIGN | MEMINFO | GFXINFO  # this is the Multiboot 'flag' field
.set MAGIC,    0x1BADB002       # 'magic number' lets bootloader find the header
.set CHECKSUM, -(MAGIC + FLAGS) # checksum of above, to prove we are multiboot

# Declare a header as in the Multiboot Standard.
.section .multiboot.data
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM
.long 0, 0, 0, 0, 0

# Video Mode Settings
.long 0
.long 1024
.long 768
.long 32

# Reserve 16KiB stack for the initial thread.
.section .bootstrap_stack, "aw", @nobits
.align 16
stack_bottom:
	.skip 16384
stack_top:

# The kernel entry point.
.section .multiboot.text
.global _start
.type _start, @function
_start:
	movl (initial_page_dir - 0xC0000000), %eax
	movl %eax, %cr3

	# Enable PAE.
	movl %cr4, %ecx
	orl $0x10, %ecx
	movl %ecx, %cr4 

	# Enable paging.
	movl %cr0, %ecx
	orl $0x80000000, %ecx
	movl %ecx, %cr0

	jmp higher_half


.section .text
.globl higher_half
.extern kernel_main
.type higher_half, @function
higher_half:
	movl $stack_top, %esp

	# Call the global constructors.
	call _init

	# Transfer control to the main kernel.
	pushl %ebx
	xorl %ebp, %ebp
	call kernel_main

	# Halt the CPU if kernel_main returns.
	hlt


# The initial page directory.
.section .data
.align 4096
.global initial_page_dir
initial_page_dir:
    .long 0b10000011  # The first entry, present, RW, supervisor
    .rept 768-1       # Fill the next 767 entries with 0
    .long 0
    .endr

    # Identity mapping of first 4 MB pages
    .long (0 << 22) | 0b10000011  # Page 0, present, RW, supervisor
    .long (1 << 22) | 0b10000011  # Page 1, present, RW, supervisor
    .long (2 << 22) | 0b10000011  # Page 2, present, RW, supervisor
    .long (3 << 22) | 0b10000011  # Page 3, present, RW, supervisor

    # Fill the remaining entries of the directory
    .rept 256-4       # 252 entries remaining
    .long 0
    .endr