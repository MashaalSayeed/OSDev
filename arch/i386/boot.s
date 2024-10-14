.set ALIGN,    1<<0             # align loaded modules on page boundaries
.set MEMINFO,  1<<1             # provide memory map
.set GFXINFO,  1<<2             # provide video information

.set FLAGS,    ALIGN | MEMINFO | GFXINFO  # this is the Multiboot 'flag' field
.set MAGIC,    0x1BADB002       # 'magic number' lets bootloader find the header
.set CHECKSUM, -(MAGIC + FLAGS) # checksum of above, to prove we are multiboot

# Declare a header as in the Multiboot Standard.
.section .multiboot
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

# Reserve a stack for the initial thread.
.section .bss
.align 16
stack_bottom:
.skip 16384 # 16 KiB
stack_top:

# The kernel entry point.
.section .text
.global _start
.type _start, @function
_start:
	movl $stack_top, %esp

	# Call the global constructors.
	call _init

	# Transfer control to the main kernel.
	pushl %ebx
	pushl %eax
	call kernel_main

	# Hang if kernel_main unexpectedly returns.
	cli
1:	hlt
	jmp 1b
.size _start, . - _start
