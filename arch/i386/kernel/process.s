; Segment selectors
USER_CS equ 0x1B
USER_DS equ 0x23

; thread_t struct offsets
THREAD_EIP      equ 4
THREAD_USER_ESP equ 16
THREAD_GS       equ 20

; void switch_context(thread_t* context)
global switch_context
switch_context:
    mov eax, [esp+4]    ; eax = thread_t*

    ; Load user data segments
    mov cx, USER_DS
    mov ds, cx
    mov es, cx
    mov fs, cx

    ; Restore gs from thread->gs, default to USER_DS if 0
    movzx ecx, word [eax+THREAD_GS]
    test ecx, ecx
    jnz .set_gs
    mov ecx, USER_DS
.set_gs:
    mov gs, cx

    ; Set up the stack for IRET to transition to user space
    push dword USER_DS                      ; SS
    push dword [eax+THREAD_USER_ESP]        ; USER ESP
    push dword 0x202                        ; EFLAGS (Interrupts enabled, IOPL=0)
    push dword USER_CS                      ; CS
    push dword [eax+THREAD_EIP]             ; EIP
    
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