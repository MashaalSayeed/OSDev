; Very small C runtime startup
; Assumes: ESP points to argc at program entry

global _start
extern main
extern exit

section .text
_start:
    mov ebx, [esp]  ; argc
    lea ecx, [esp + 12] ; argv points to the next item on the stack
    push ecx             ; push argv for main
    push ebx             ; push argc for main
    
    call main

    mov ebx, eax        ; move return value to ebx
    mov eax, 60
    int 0x80            ; syscall to exit with return value in ebx