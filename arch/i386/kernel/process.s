; Segment selectors
USER_CS equ 0x1B
USER_DS equ 0x23

; void switch_context(uintptr_t entry_point, uintptr_t user_esp, uint32_t gs)
global switch_context
switch_context:
    ; Load arguments into registers before we modify the stack pointer
    mov eax, [esp+4]    ; eax = entry_point
    mov ebx, [esp+8]    ; ebx = user_esp
    movzx ecx, word [esp+12] ; ecx = gs

    ; Load user data segments
    mov dx, USER_DS
    mov ds, dx
    mov es, dx
    mov fs, dx

    ; Restore gs, default to USER_DS if 0
    test ecx, ecx
    jnz .set_gs
    mov ecx, USER_DS
.set_gs:
    mov gs, cx

    ; Set up the stack for IRET to transition to user space
    push dword USER_DS                      ; SS
    push ebx                                ; USER ESP
    push dword 0x202                        ; EFLAGS (Interrupts enabled, IOPL=0)
    push dword USER_CS                      ; CS
    push eax                                ; EIP
    
    iret


; void switch_task(uint32_t *prev_stack, uint32_t next_stack);
global switch_task
switch_task:
    ; Fetch arguments before we modify the stack pointer
    mov ecx, [esp+4]    ; ecx = prev_stack
    mov edx, [esp+8]    ; edx = next_stack

    ; Save context of the outgoing thread
    pushad
    push gs

    ; Switch stack pointers
    mov [ecx], esp      ; Save current ESP in prev_thread context
    mov esp, edx        ; Restore ESP from next_thread context

    ; Restore context of the incoming thread
    pop gs
    popad
    ret

global fork_trampoline
fork_trampoline:
    pop ds
    pop es
    pop fs
    pop gs
    popad
    add esp, 8 
    iret

global read_eip
read_eip:
	pop eax
	jmp eax